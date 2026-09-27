// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "sound_player.hpp"

#include "assets.hpp"

#include <dlfcn.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

struct PaSampleSpec {
    int format;
    std::uint32_t rate;
    std::uint8_t channels;
};

using PaSimpleNew = void *(*)(const char *, const char *, int, const char *, const char *,
                              const PaSampleSpec *, const void *, const void *, int *);
using PaSimpleWrite = int (*)(void *, const void *, std::size_t, int *);
using PaSimpleFree = void (*)(void *);

struct PulseApi {
    void *lib = nullptr;
    PaSimpleNew news = nullptr;
    PaSimpleWrite write = nullptr;
    PaSimpleFree free_fn = nullptr;
    bool tried = false;
    bool ok = false;
};

PulseApi &pulse_api()
{
    static PulseApi api;
    if (api.tried) {
        return api;
    }
    api.tried = true;
    api.lib = dlopen("libpulse-simple.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (api.lib == nullptr) {
        return api;
    }
    api.news = reinterpret_cast<PaSimpleNew>(dlsym(api.lib, "pa_simple_new"));
    api.write = reinterpret_cast<PaSimpleWrite>(dlsym(api.lib, "pa_simple_write"));
    api.free_fn = reinterpret_cast<PaSimpleFree>(dlsym(api.lib, "pa_simple_free"));
    api.ok = api.news != nullptr && api.write != nullptr && api.free_fn != nullptr;
    return api;
}

std::string normalize_name(std::string text)
{
    std::string out;
    out.reserve(text.size());
    for (unsigned char ch : text) {
        if (ch == '.' || ch == '-' || ch == '_' || ch == ' ') {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<unsigned char>(ch - 'A' + 'a');
        }
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

struct Clip {
    int rate = 22050;
    std::vector<std::int16_t> samples;
};

bool parse_wav(const std::string &path, Clip &clip)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    if (data.size() < 44 || std::memcmp(data.data(), "RIFF", 4) != 0 ||
        std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
        return false;
    }

    int format = 0;
    int channels = 0;
    int rate = 0;
    int bits = 0;
    const unsigned char *pcm = nullptr;
    std::size_t pcm_bytes = 0;

    std::size_t pos = 12;
    while (pos + 8 <= data.size()) {
        const unsigned char *chunk = data.data() + pos;
        const std::uint32_t size = static_cast<std::uint32_t>(chunk[4]) |
                                   (static_cast<std::uint32_t>(chunk[5]) << 8) |
                                   (static_cast<std::uint32_t>(chunk[6]) << 16) |
                                   (static_cast<std::uint32_t>(chunk[7]) << 24);
        const unsigned char *body = chunk + 8;
        if (pos + 8 + size > data.size()) {
            break;
        }
        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            format = body[0] | (body[1] << 8);
            channels = body[2] | (body[3] << 8);
            rate = static_cast<int>(body[4] | (body[5] << 8) | (body[6] << 16) | (body[7] << 24));
            bits = body[14] | (body[15] << 8);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            pcm = body;
            pcm_bytes = size;
        }
        pos += 8 + size + (size & 1u);
    }

    // Format 1 is linear PCM. μ-law files are kept on disk but not played.
    if (format != 1 || pcm == nullptr || channels < 1 || channels > 2 || rate < 1000) {
        return false;
    }
    if (bits != 8 && bits != 16) {
        return false;
    }

    clip.rate = rate;
    clip.samples.clear();
    if (bits == 16) {
        const std::size_t frames = pcm_bytes / (static_cast<std::size_t>(channels) * 2);
        clip.samples.reserve(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            int mixed = 0;
            for (int c = 0; c < channels; ++c) {
                const std::size_t off = (i * static_cast<std::size_t>(channels) + c) * 2;
                int sample = pcm[off] | (pcm[off + 1] << 8);
                if (sample & 0x8000) {
                    sample -= 0x10000;
                }
                mixed += sample;
            }
            clip.samples.push_back(static_cast<std::int16_t>(mixed / channels));
        }
    } else {
        const std::size_t frames = pcm_bytes / static_cast<std::size_t>(channels);
        clip.samples.reserve(frames);
        for (std::size_t i = 0; i < frames; ++i) {
            int mixed = 0;
            for (int c = 0; c < channels; ++c) {
                const int sample = static_cast<int>(pcm[i * static_cast<std::size_t>(channels) + c]) - 128;
                mixed += sample << 8;
            }
            clip.samples.push_back(static_cast<std::int16_t>(mixed / channels));
        }
    }
    return !clip.samples.empty();
}

const std::unordered_map<std::string, std::string> &sound_index()
{
    static const std::unordered_map<std::string, std::string> index = [] {
        std::unordered_map<std::string, std::string> map;
        const std::string dir = asset_root().empty() ? std::string() : asset_root() + "/res/sounds";
        if (dir.empty()) {
            return map;
        }
        std::error_code ec;
        for (const auto &entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec || !entry.is_regular_file()) {
                continue;
            }
            const auto name = entry.path().filename().string();
            if (name.size() < 5 || name.substr(name.size() - 4) != ".wav") {
                continue;
            }
            Clip probe;
            if (!parse_wav(entry.path().string(), probe)) {
                continue;
            }
            const std::string key = normalize_name(name.substr(0, name.size() - 4));
            const auto found = map.find(key);
            if (found == map.end() || entry.file_size() > std::filesystem::file_size(found->second, ec)) {
                map[key] = entry.path().string();
            }
        }
        return map;
    }();
    return index;
}

bool open_device(int rate, void *&stream)
{
    PulseApi &api = pulse_api();
    if (!api.ok) {
        return false;
    }
    PaSampleSpec spec;
    spec.format = 3; // PA_SAMPLE_S16LE
    spec.rate = static_cast<std::uint32_t>(rate);
    spec.channels = 1;
    int err = 0;
    stream = api.news(nullptr, "Lunduke City", 1 /* PA_STREAM_PLAYBACK */, nullptr, "city", &spec,
                      nullptr, nullptr, &err);
    return stream != nullptr;
}

void playback_thread(Clip clip)
{
    void *stream = nullptr;
    if (!open_device(clip.rate, stream)) {
        return;
    }
    PulseApi &api = pulse_api();
    int err = 0;
    api.write(stream, clip.samples.data(), clip.samples.size() * sizeof(std::int16_t), &err);
    api.free_fn(stream);
}

} // namespace

SoundPlayer::SoundPlayer() = default;

SoundPlayer::~SoundPlayer()
{
    for (auto &job : jobs_) {
        if (job.thread.joinable()) {
            job.thread.join();
        }
    }
}

void SoundPlayer::set_muted(bool muted)
{
    muted_ = muted;
}

bool SoundPlayer::probe()
{
    if (probed_) {
        return device_ok_;
    }
    probed_ = true;
    if (!pulse_api().ok) {
        device_ok_ = false;
        return false;
    }
    void *stream = nullptr;
    device_ok_ = open_device(22050, stream);
    if (stream != nullptr) {
        pulse_api().free_fn(stream);
    }
    return device_ok_;
}

void SoundPlayer::reap()
{
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        if (it->done && it->done->load()) {
            if (it->thread.joinable()) {
                it->thread.join();
            }
            it = jobs_.erase(it);
        } else {
            ++it;
        }
    }
}

bool SoundPlayer::play(const std::string &engine_name)
{
    if (muted_ || engine_name.empty()) {
        return false;
    }
    std::string key = normalize_name(engine_name);
    // Upstream res/sounds has no fog-horn sample. Skip it rather than substitute.
    if (key == "foghornlow") {
        return false;
    }
    const auto &index = sound_index();
    const auto found = index.find(key);
    if (found == index.end()) {
        return false;
    }
    if (!probe() || !device_ok_) {
        return false;
    }

    Clip clip;
    if (!parse_wav(found->second, clip)) {
        return false;
    }

    reap();
    if (jobs_.size() >= 3) {
        return false;
    }
    auto done = std::make_shared<std::atomic<bool>>(false);
    Job job;
    job.done = done;
    job.thread = std::thread([clip = std::move(clip), done]() mutable {
        playback_thread(std::move(clip));
        done->store(true);
    });
    jobs_.push_back(std::move(job));
    return true;
}

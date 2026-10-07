// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "sound_player.hpp"

#include "assets.hpp"

#include <dlfcn.h>

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
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

constexpr int kMixRate = 22050;

std::vector<std::int16_t> resample(const std::vector<std::int16_t> &in, int from_rate, int to_rate)
{
    if (in.empty() || from_rate <= 0 || to_rate <= 0 || from_rate == to_rate) {
        return in;
    }
    const double step = static_cast<double>(from_rate) / static_cast<double>(to_rate);
    const auto out_n = static_cast<std::size_t>(static_cast<double>(in.size()) / step);
    std::vector<std::int16_t> out;
    out.reserve(out_n);
    for (std::size_t i = 0; i < out_n; ++i) {
        const double pos = static_cast<double>(i) * step;
        const auto i0 = static_cast<std::size_t>(pos);
        if (i0 >= in.size()) {
            break;
        }
        const auto i1 = std::min(i0 + 1, in.size() - 1);
        const double frac = pos - static_cast<double>(i0);
        const double mixed = static_cast<double>(in[i0]) * (1.0 - frac) + static_cast<double>(in[i1]) * frac;
        int sample = static_cast<int>(mixed);
        if (sample > 32767) {
            sample = 32767;
        }
        if (sample < -32768) {
            sample = -32768;
        }
        out.push_back(static_cast<std::int16_t>(sample));
    }
    return out;
}

// Paths only. The PCM itself is decoded once, on the mixer thread.
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

} // namespace

struct SoundPlayer::Mixer {
    struct Voice {
        std::shared_ptr<const std::vector<std::int16_t>> samples;
        std::size_t pos = 0;
    };

    std::mutex mu;
    std::condition_variable cv;
    std::vector<Voice> voices;
    std::vector<std::string> pending;
    std::mutex clip_mu;
    std::unordered_map<std::string, std::shared_ptr<const std::vector<std::int16_t>>> clips;
    std::thread thread;
    std::atomic<bool> stop{false};
    bool muted = false;
    bool started = false;

    ~Mixer()
    {
        stop.store(true);
        cv.notify_all();
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::shared_ptr<const std::vector<std::int16_t>> clip_for(const std::string &path)
    {
        {
            std::lock_guard<std::mutex> lock(clip_mu);
            const auto found = clips.find(path);
            if (found != clips.end()) {
                return found->second;
            }
        }
        Clip parsed;
        if (!parse_wav(path, parsed)) {
            return {};
        }
        auto samples = std::make_shared<const std::vector<std::int16_t>>(resample(parsed.samples, parsed.rate, kMixRate));
        if (samples->empty()) {
            return {};
        }
        std::lock_guard<std::mutex> lock(clip_mu);
        const auto found = clips.find(path);
        if (found != clips.end()) {
            return found->second;
        }
        clips.emplace(path, samples);
        return samples;
    }

    void request(const std::string &path)
    {
        std::lock_guard<std::mutex> lock(mu);
        if (muted || stop.load()) {
            return;
        }
        pending.push_back(path);
        cv.notify_all();
    }

    void set_muted(bool on)
    {
        std::lock_guard<std::mutex> lock(mu);
        muted = on;
        if (on) {
            voices.clear();
            pending.clear();
        }
        cv.notify_all();
    }

    void loop()
    {
        void *stream = nullptr;
        if (!open_device(kMixRate, stream)) {
            return;
        }
        constexpr std::size_t kChunk = 512;
        std::vector<std::int32_t> acc(kChunk);
        std::vector<std::int16_t> out(kChunk);
        while (!stop.load()) {
            std::vector<std::string> todo;
            {
                std::unique_lock<std::mutex> lock(mu);
                if (voices.empty() && pending.empty()) {
                    cv.wait(lock, [&] { return stop.load() || !pending.empty(); });
                }
                if (stop.load()) {
                    break;
                }
                if (muted) {
                    voices.clear();
                    pending.clear();
                    continue;
                }
                todo.swap(pending);
            }
            for (const auto &path : todo) {
                if (stop.load()) {
                    break;
                }
                auto clip = clip_for(path);
                if (!clip) {
                    continue;
                }
                std::lock_guard<std::mutex> lock(mu);
                if (muted || stop.load()) {
                    break;
                }
                voices.push_back(Voice{std::move(clip), 0});
            }
            bool have = false;
            {
                std::lock_guard<std::mutex> lock(mu);
                if (stop.load()) {
                    break;
                }
                if (muted) {
                    voices.clear();
                    continue;
                }
                if (voices.empty()) {
                    continue;
                }
                std::fill(acc.begin(), acc.end(), 0);
                for (auto it = voices.begin(); it != voices.end();) {
                    const auto &samples = *it->samples;
                    for (std::size_t i = 0; i < acc.size() && it->pos < samples.size(); ++i, ++it->pos) {
                        acc[i] += samples[it->pos];
                    }
                    if (it->pos >= samples.size()) {
                        it = voices.erase(it);
                    } else {
                        ++it;
                    }
                }
                for (std::size_t i = 0; i < out.size(); ++i) {
                    int sample = acc[i];
                    if (sample > 32767) {
                        sample = 32767;
                    }
                    if (sample < -32768) {
                        sample = -32768;
                    }
                    out[i] = static_cast<std::int16_t>(sample);
                }
                have = true;
            }
            if (!have || stop.load()) {
                continue;
            }
            int err = 0;
            if (pulse_api().write(stream, out.data(), out.size() * sizeof(std::int16_t), &err) < 0) {
                break;
            }
        }
        if (stream != nullptr) {
            pulse_api().free_fn(stream);
        }
    }
};

SoundPlayer::SoundPlayer() = default;

SoundPlayer::~SoundPlayer() = default;

void SoundPlayer::set_muted(bool muted)
{
    const bool was = muted_.exchange(muted);
    if (mixer_) {
        mixer_->set_muted(muted);
    }
    (void)was;
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
    device_ok_ = open_device(kMixRate, stream);
    if (stream != nullptr) {
        pulse_api().free_fn(stream);
    }
    return device_ok_;
}

void SoundPlayer::ensure_mixer()
{
    if (!mixer_) {
        mixer_ = std::make_unique<Mixer>();
    }
    if (mixer_->started) {
        return;
    }
    mixer_->started = true;
    mixer_->muted = muted_.load();
    Mixer *mixer = mixer_.get();
    mixer_->thread = std::thread([mixer] { mixer->loop(); });
}

bool SoundPlayer::play(const std::string &engine_name)
{
    if (muted_.load() || engine_name.empty()) {
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
    ensure_mixer();
    mixer_->request(found->second);
    return true;
}

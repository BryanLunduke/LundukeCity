// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

// A couple of megabytes covers the shipped clips. Larger files, and
// symlinks to them, are not decoded.
inline constexpr std::size_t kMaxWavBytes = 2u * 1024u * 1024u;
// The mixer drops the oldest request once the queue reaches this length.
inline constexpr std::size_t kMaxQueuedSounds = 8;

// Regular files only, first name wins, nothing over kMaxWavBytes.
std::unordered_map<std::string, std::string> index_sound_directory(const std::string &dir);
// False when the file is missing, not a regular file, too large, or not PCM.
bool preview_wav_file(const std::string &path);

// Plays Micropolis wav assets when the engine asks for a sound by name.
// Each clip is decoded once. A single worker mixes them into one Pulse
// stream. Mute and destruction stop that worker without waiting for the
// rest of a clip to drain. If PulseAudio is missing or no device can be
// opened, play() returns without throwing.
class SoundPlayer {
public:
    SoundPlayer();
    ~SoundPlayer();

    SoundPlayer(const SoundPlayer &) = delete;
    SoundPlayer &operator=(const SoundPlayer &) = delete;

    void set_muted(bool muted);
    bool muted() const { return muted_.load(); }

    // Opens the sound library and tries one silent stream. Safe when no
    // audio device exists. Returns whether a device accepted the stream.
    bool probe();

    // engine_name is the Micropolis sound id, for example "Siren".
    // Returns false when muted, unknown, or no device. Never throws.
    bool play(const std::string &engine_name);

private:
    struct Mixer;

    void ensure_mixer();

    std::atomic<bool> muted_{false};
    bool probed_ = false;
    bool device_ok_ = false;
    std::unique_ptr<Mixer> mixer_;
};

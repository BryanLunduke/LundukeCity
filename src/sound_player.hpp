// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <atomic>
#include <memory>
#include <string>

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

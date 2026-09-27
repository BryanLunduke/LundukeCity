// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "sprite_art.hpp"

#include "assets.hpp"

#include <gdkmm/general.h>
#include <gdkmm/pixbuf.h>

#include <iostream>
#include <string>
#include <vector>

namespace {

struct Frame {
    Glib::RefPtr<Gdk::Pixbuf> pix;
    Cairo::RefPtr<Cairo::ImageSurface> surface;
};

constexpr int kTypes = 8;
constexpr int kFrames = 16;

Frame &slot(int type, int frame_index)
{
    static std::vector<Frame> frames(static_cast<std::size_t>(kTypes * kFrames));
    return frames[static_cast<std::size_t>((type - 1) * kFrames + frame_index)];
}

bool load_frame(int type, int frame_index)
{
    Frame &frame = slot(type, frame_index);
    if (frame.surface) {
        return true;
    }
    const std::string relative = "images/micropolisEngine/obj" + std::to_string(type) + "-" +
                                 std::to_string(frame_index) + ".png";
    const std::string path = asset_file(relative);
    if (path.empty()) {
        return false;
    }
    try {
        frame.pix = Gdk::Pixbuf::create_from_file(path);
        if (frame.pix && !frame.pix->get_has_alpha()) {
            frame.pix = frame.pix->add_alpha(false, 0, 0, 0);
        }
        frame.surface = Gdk::Cairo::create_surface_from_pixbuf(frame.pix, 1);
    } catch (const Glib::Error &err) {
        std::cerr << "Lunduke City: could not read sprite " << relative << ": " << err.what()
                  << "\n";
        frame.surface.clear();
        return false;
    }
    return static_cast<bool>(frame.surface);
}

} // namespace

Cairo::RefPtr<Cairo::ImageSurface> sprite_frame(int type, int frame)
{
    if (type < 1 || type > kTypes || frame < 1) {
        return {};
    }
    int index = frame - 1;
    if (index >= kFrames) {
        index = kFrames - 1;
    }
    if (!load_frame(type, index)) {
        // Engine frames sometimes run one past a short cycle; use the last real image.
        for (int back = index - 1; back >= 0; --back) {
            if (load_frame(type, back) || slot(type, back).surface) {
                return slot(type, back).surface;
            }
        }
        return {};
    }
    return slot(type, index).surface;
}

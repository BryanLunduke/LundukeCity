// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "city_session.hpp"

#include <cairomm/surface.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>

#include <vector>

class OverlayWindow : public Gtk::Window {
public:
    explicit OverlayWindow(CitySession::MapLayer layer);

    void set_session(CitySession *session);
    void present_map();
    // Rebuild the retained image when the map or a density layer changed.
    void sync();

protected:
    bool on_draw_map(const Cairo::RefPtr<Cairo::Context> &cr);

private:
    bool density_layer() const;
    void rebuild();

    CitySession::MapLayer layer_;
    CitySession *session_ = nullptr;
    Cairo::RefPtr<Cairo::ImageSurface> image_;
    std::vector<int> cells_;
    int peak_ = 1;
    unsigned map_serial_ = 0;
    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 6};
    Gtk::Label legend_;
    Gtk::DrawingArea map_;
};

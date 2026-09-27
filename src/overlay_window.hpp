// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "city_session.hpp"

#include <gtkmm/drawingarea.h>
#include <gtkmm/label.h>
#include <gtkmm/window.h>
#include <gtkmm/box.h>

class OverlayWindow : public Gtk::Window {
public:
    explicit OverlayWindow(CitySession::MapLayer layer);

    void set_session(CitySession *session);
    void present_map();

protected:
    bool on_draw_map(const Cairo::RefPtr<Cairo::Context> &cr);

private:
    CitySession::MapLayer layer_;
    CitySession *session_ = nullptr;
    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 6};
    Gtk::Label legend_;
    Gtk::DrawingArea map_;
};

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/label.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/window.h>

class CitySession;

// History charts for the engine census: population (from the residential,
// commercial, and industrial series), cash flow, and the other history tables.
class GraphsWindow : public Gtk::Window {
public:
    GraphsWindow();

    void set_session(CitySession *session);
    void present_graphs();
    void sync();

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr);

    CitySession *session_ = nullptr;
    Gtk::RadioButtonGroup scale_group_;
    Gtk::RadioButton ten_;
    Gtk::RadioButton long_term_;

    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 8};
    Gtk::Box scales_{Gtk::ORIENTATION_HORIZONTAL, 12};
    Gtk::Label population_;
    Gtk::Label funds_;
    Gtk::Label scale_note_;
    Gtk::DrawingArea chart_;
};

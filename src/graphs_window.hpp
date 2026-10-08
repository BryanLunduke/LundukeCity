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

    static constexpr int kSeriesCount = 7;

private:
    // Not named on_draw: that would override Gtk::Window and skip the labels.
    bool on_chart_draw(const Cairo::RefPtr<Cairo::Context> &cr);
    bool chart_is_dark() const;
    void refresh_legend();

    CitySession *session_ = nullptr;
    // The header and the Population legend both show this census figure.
    bool have_census_ = false;
    long shown_population_ = 0;
    Gtk::RadioButtonGroup scale_group_;
    Gtk::RadioButton ten_;
    Gtk::RadioButton long_term_;

    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 8};
    Gtk::Box scales_{Gtk::ORIENTATION_HORIZONTAL, 12};
    Gtk::Label population_;
    Gtk::Label funds_;
    Gtk::Label scale_note_;
    Gtk::DrawingArea chart_;
    // The legend is widgets, not cairo text on the chart. The chart has its
    // own window; text drawn there in the theme foreground disappeared.
    Gtk::Label *legend_label_[kSeriesCount] = {};
    Gtk::DrawingArea *legend_swatch_[kSeriesCount] = {};
    double swatch_r_[kSeriesCount] = {};
    double swatch_g_[kSeriesCount] = {};
    double swatch_b_[kSeriesCount] = {};
    double swatch_dash_on_[kSeriesCount] = {};
    double swatch_dash_off_[kSeriesCount] = {};
};

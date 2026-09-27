// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <functional>
#include <gtkmm/drawingarea.h>

class CitySession;

class MinimapView : public Gtk::DrawingArea {
public:
    MinimapView();

    void set_session(CitySession *session);

    // Visible window as fractions of the whole map: x, y, width, height.
    void set_viewport_provider(std::function<void(double &, double &, double &, double &)> provider);

    sigc::signal<void, double, double> signal_jump;

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;

private:
    CitySession *session_ = nullptr;
    std::function<void(double &, double &, double &, double &)> viewport_;
};

class DemandView : public Gtk::DrawingArea {
public:
    DemandView();

    void set_session(CitySession *session);

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;

private:
    CitySession *session_ = nullptr;
};

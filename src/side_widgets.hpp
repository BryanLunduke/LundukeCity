// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "view_math.hpp"

#include <cairomm/surface.h>
#include <functional>
#include <gtkmm/drawingarea.h>
#include <vector>

class CitySession;

class MinimapView : public Gtk::DrawingArea {
public:
    MinimapView();

    void set_session(CitySession *session);

    // Visible window as fractions of the whole map: x, y, width, height.
    void set_viewport_provider(std::function<void(double &, double &, double &, double &)> provider);

    // Write average colors for tiles whose map word changed, then paint
    // the retained buffer. Scrolling calls invalidate_viewport() instead.
    void sync();
    void invalidate_viewport();

    sigc::signal<void, double, double> signal_jump;

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;
    void on_size_allocate(Gtk::Allocation &allocation) override;

private:
    AspectBox land_box() const;
    void paint_viewport(const Cairo::RefPtr<Cairo::Context> &cr) const;
    void queue_viewport_rect(int x, int y, int width, int height);

    CitySession *session_ = nullptr;
    std::function<void(double &, double &, double &, double &)> viewport_;
    Cairo::RefPtr<Cairo::ImageSurface> pixels_;
    std::vector<int> cached_raw_;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    int viewport_w_ = 0;
    int viewport_h_ = 0;
    bool viewport_valid_ = false;
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

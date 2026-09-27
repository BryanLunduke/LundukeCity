// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/drawingarea.h>

class CitySession;

// Top-down city viewport. Tiles are drawn procedurally from engine map words.
class MapView : public Gtk::DrawingArea {
public:
    MapView();

    void set_session(CitySession *session);
    void set_tool(int engine_tool);

    void set_tile_size(int pixels);
    int tile_size() const { return tile_size_; }
    int pixel_width() const;
    int pixel_height() const;

    sigc::signal<void, int, int> signal_tool_down;
    sigc::signal<void, int, int, int, int> signal_tool_drag;

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_button_release_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;

private:
    bool tile_at(double x, double y, int &tx, int &ty) const;

    CitySession *session_ = nullptr;
    int engine_tool_ = 6;
    int tile_size_ = 10;
    bool dragging_ = false;
    int last_x_ = -1;
    int last_y_ = -1;
};

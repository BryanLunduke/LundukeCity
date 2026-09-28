// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cairomm/surface.h>
#include <gtkmm/drawingarea.h>

#include <chrono>

class CitySession;

// Top-down city viewport. Tiles are drawn from the Micropolis 16-pixel atlas.
class MapView : public Gtk::DrawingArea {
public:
    MapView();

    void set_session(CitySession *session);
    void set_tool(int engine_tool);

    // Tile under the pointer, used to outline a placeable tool's footprint.
    // Cleared when the pointer leaves the map.
    void set_hover_tile(int tx, int ty);
    void clear_hover();

    void set_tile_size(int pixels);
    int tile_size() const { return tile_size_; }
    int pixel_width() const;
    int pixel_height() const;

    sigc::signal<void, int, int> signal_tool_down;
    sigc::signal<void, int, int, int, int> signal_tool_drag;

protected:
    void on_realize() override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_button_release_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;
    bool on_leave_notify_event(GdkEventCrossing *event) override;

private:
    bool tile_at(double x, double y, int &tx, int &ty) const;
    void draw_footprint(const Cairo::RefPtr<Cairo::Context> &cr) const;

    CitySession *session_ = nullptr;
    int engine_tool_ = 6;
    int tile_size_ = 16;
    bool dragging_ = false;
    int last_x_ = -1;
    int last_y_ = -1;
    bool hover_valid_ = false;
    int hover_x_ = 0;
    int hover_y_ = 0;
    bool blink_on_ = false;
    std::chrono::steady_clock::time_point blink_stamp_{};
    Cairo::RefPtr<Cairo::ImageSurface> map_pixels_;
};

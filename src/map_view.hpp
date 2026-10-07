// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "city_session.hpp"

#include <cairomm/surface.h>
#include <gtkmm/drawingarea.h>

#include <chrono>
#include <vector>

class TileAtlas;

// Top-down city viewport. Tiles are drawn from the Micropolis 16-pixel atlas.
// The ARGB buffer is kept. A draw copies only tiles whose map word changed,
// plus unpowered zones the lightning blink toggles.
class MapView : public Gtk::DrawingArea {
public:
    MapView();
    ~MapView() override;

    void set_session(CitySession *session);
    void set_tool(int engine_tool);

    // Copy changed tiles into the retained buffer and invalidate those
    // rectangles. Hover outlines do not call this.
    void sync();

    // Tile under the pointer, used to outline a placeable tool's footprint.
    // Cleared when the pointer leaves the map.
    void set_hover_tile(int tx, int ty);
    void clear_hover();

    void set_tile_size(int pixels);
    // Pixel offset applied while an earthquake is shaking the map.
    void set_shake(int dx, int dy);
    int tile_size() const { return tile_size_; }
    int pixel_width() const;
    int pixel_height() const;

    sigc::signal<void, int, int> signal_tool_down;
    sigc::signal<void, int, int, int, int> signal_tool_drag;
    // Ctrl+= / Ctrl+- while the map itself has keyboard focus.
    sigc::signal<void, int> signal_zoom;

protected:
    void on_realize() override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_button_release_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;
    bool on_leave_notify_event(GdkEventCrossing *event) override;
    bool on_key_press_event(GdkEventKey *event) override;

private:
    bool tile_at(double x, double y, int &tx, int &ty) const;
    void draw_footprint(const Cairo::RefPtr<Cairo::Context> &cr) const;
    void ensure_surface();
    int display_tile(int raw, const TileAtlas &atlas) const;
    void blit_tile(int x, int y, int shown, const TileAtlas &atlas);
    void invalidate_tile(int x, int y);
    void invalidate_footprint(int tx, int ty, int engine_tool);
    void invalidate_sprite(const CitySession::SpriteDot &dot);
    void end_drag();
    void grab_pointer(GdkEventButton *event);
    void ungrab_pointer();
    bool scroll_at_edge(double root_x, double root_y);
    void follow_root_pointer(double root_x, double root_y, bool drag);
    void follow_local_pointer(double x, double y, bool drag);
    bool on_blink();
    bool on_edge_scroll();

    CitySession *session_ = nullptr;
    int engine_tool_ = 6;
    int tile_size_ = 16;
    int shake_x_ = 0;
    int shake_y_ = 0;
    bool dragging_ = false;
    bool pointer_grabbed_ = false;
    int last_x_ = -1;
    int last_y_ = -1;
    double last_root_x_ = 0;
    double last_root_y_ = 0;
    bool hover_valid_ = false;
    int hover_x_ = 0;
    int hover_y_ = 0;
    bool blink_on_ = false;
    std::vector<int> cached_raw_;
    std::vector<int> cached_shown_;
    std::vector<CitySession::SpriteDot> cached_sprites_;
    sigc::connection blink_timer_;
    sigc::connection edge_timer_;
    Cairo::RefPtr<Cairo::ImageSurface> map_pixels_;
};

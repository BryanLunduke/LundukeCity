// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "map_view.hpp"

#include "city_session.hpp"
#include "sprite_art.hpp"
#include "tile_atlas.hpp"
#include "tools.hpp"
#include "zoom_keys.hpp"

#include "micropolis.h"

#include <gdkmm/cursor.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

MapView::MapView()
{
    set_can_focus(true);
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
               Gdk::BUTTON1_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::KEY_PRESS_MASK);
    set_tile_size(tile_size_);
}

void MapView::on_realize()
{
    Gtk::DrawingArea::on_realize();
    if (auto window = get_window()) {
        window->set_cursor(Gdk::Cursor::create(get_display(), Gdk::CROSSHAIR));
    }
}

void MapView::set_session(CitySession *session)
{
    session_ = session;
    queue_draw();
}

void MapView::set_tool(int engine_tool)
{
    engine_tool_ = engine_tool;
    queue_draw();
}

void MapView::set_hover_tile(int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= CitySession::kWorldW || ty >= CitySession::kWorldH) {
        clear_hover();
        return;
    }
    if (hover_valid_ && hover_x_ == tx && hover_y_ == ty) {
        return;
    }
    hover_valid_ = true;
    hover_x_ = tx;
    hover_y_ = ty;
    queue_draw();
}

void MapView::clear_hover()
{
    if (!hover_valid_) {
        return;
    }
    hover_valid_ = false;
    queue_draw();
}

void MapView::set_tile_size(int pixels)
{
    tile_size_ = std::max(6, std::min(16, pixels));
    set_size_request(pixel_width(), pixel_height());
    queue_draw();
}

int MapView::pixel_width() const
{
    return CitySession::kWorldW * tile_size_;
}

int MapView::pixel_height() const
{
    return CitySession::kWorldH * tile_size_;
}

bool MapView::tile_at(double x, double y, int &tx, int &ty) const
{
    if (tile_size_ <= 0) {
        return false;
    }
    tx = static_cast<int>(x) / tile_size_;
    ty = static_cast<int>(y) / tile_size_;
    return tx >= 0 && ty >= 0 && tx < CitySession::kWorldW && ty < CitySession::kWorldH;
}

bool MapView::on_button_press_event(GdkEventButton *event)
{
    if (event->button != 1) {
        return false;
    }
    int tx = 0;
    int ty = 0;
    if (!tile_at(event->x, event->y, tx, ty)) {
        return false;
    }
    dragging_ = true;
    last_x_ = tx;
    last_y_ = ty;
    // A click on the map leaves it focused, so the next Ctrl+= / Ctrl+-
    // is delivered here as well as through the window accelerator.
    grab_focus();
    signal_tool_down.emit(tx, ty);
    return true;
}

bool MapView::on_button_release_event(GdkEventButton *event)
{
    if (event->button == 1) {
        dragging_ = false;
    }
    return true;
}

bool MapView::on_motion_notify_event(GdkEventMotion *event)
{
    int tx = 0;
    int ty = 0;
    if (!tile_at(event->x, event->y, tx, ty)) {
        clear_hover();
    } else {
        set_hover_tile(tx, ty);
    }
    if (!dragging_ || (event->state & GDK_BUTTON1_MASK) == 0) {
        return true;
    }
    if (!hover_valid_) {
        return true;
    }
    if (tx == last_x_ && ty == last_y_) {
        return true;
    }
    signal_tool_drag.emit(last_x_, last_y_, tx, ty);
    last_x_ = tx;
    last_y_ = ty;
    return true;
}

bool MapView::on_leave_notify_event(GdkEventCrossing *)
{
    clear_hover();
    return true;
}

bool MapView::on_key_press_event(GdkEventKey *event)
{
    if (event != nullptr) {
        // The window also binds these keys. This path covers a key event
        // delivered straight to the focused map (Ctrl+= and Ctrl+-).
        const ZoomAction action = zoom_action(event->keyval, event->state);
        if (action == ZoomAction::In) {
            signal_zoom.emit(2);
            return true;
        }
        if (action == ZoomAction::Out) {
            signal_zoom.emit(-2);
            return true;
        }
    }
    return Gtk::DrawingArea::on_key_press_event(event);
}

void MapView::draw_footprint(const Cairo::RefPtr<Cairo::Context> &cr) const
{
    if (!hover_valid_) {
        return;
    }
    const ToolFootprint foot = tool_footprint(engine_tool_);
    if (!foot.placeable || foot.width < 1 || foot.height < 1) {
        return;
    }
    const double x = static_cast<double>(hover_x_ - foot.cursor_to_left) * tile_size_;
    const double y = static_cast<double>(hover_y_ - foot.cursor_to_top) * tile_size_;
    const double w = static_cast<double>(foot.width) * tile_size_;
    const double h = static_cast<double>(foot.height) * tile_size_;
    if (w < 2.0 || h < 2.0) {
        return;
    }
    // Stroke only. A fill would hide the tiles under the cursor.
    cr->save();
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->rectangle(x + 1.0, y + 1.0, w - 2.0, h - 2.0);
    cr->set_source_rgba(0.0, 0.0, 0.0, 0.90);
    cr->set_line_width(3.0);
    cr->stroke();
    cr->rectangle(x + 1.0, y + 1.0, w - 2.0, h - 2.0);
    cr->set_source_rgba(1.0, 1.0, 1.0, 0.95);
    cr->set_line_width(1.5);
    cr->stroke();
    cr->restore();
}

bool MapView::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    TileAtlas &atlas = tile_atlas();

    const int world_w = CitySession::kWorldW;
    const int world_h = CitySession::kWorldH;
    if (!map_pixels_ || map_pixels_->get_width() != world_w * TileAtlas::kSize ||
        map_pixels_->get_height() != world_h * TileAtlas::kSize) {
        map_pixels_ = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, world_w * TileAtlas::kSize,
                                                  world_h * TileAtlas::kSize);
    }

    const auto now = std::chrono::steady_clock::now();
    if (now - blink_stamp_ > std::chrono::milliseconds(450)) {
        blink_on_ = !blink_on_;
        blink_stamp_ = now;
    }

    unsigned char *data = map_pixels_->get_data();
    const int stride = map_pixels_->get_stride();
    const int tile_bytes = TileAtlas::kSize * 4;
    for (int y = 0; y < world_h; ++y) {
        for (int x = 0; x < world_w; ++x) {
            int raw = DIRT;
            if (session_ != nullptr) {
                raw = session_->map_value(x, y);
            }
            int tile = raw & LOMASK;
            if (blink_on_ && (raw & ZONEBIT) != 0 && (raw & PWRBIT) == 0 && atlas.loaded() &&
                LIGHTNINGBOLT < atlas.count()) {
                tile = LIGHTNINGBOLT;
            }
            unsigned char *dest = data + y * TileAtlas::kSize * stride + x * tile_bytes;
            if (atlas.loaded()) {
                atlas.blit(tile, dest, stride);
            } else {
                const std::uint32_t dirt = (255u << 24) | (204u << 16) | (127u << 8) | 102u;
                for (int row = 0; row < TileAtlas::kSize; ++row) {
                    auto *px = reinterpret_cast<std::uint32_t *>(dest + row * stride);
                    for (int col = 0; col < TileAtlas::kSize; ++col) {
                        px[col] = dirt;
                    }
                }
            }
        }
    }
    map_pixels_->mark_dirty();

    cr->save();
    const double scale = static_cast<double>(tile_size_) / static_cast<double>(TileAtlas::kSize);
    cr->scale(scale, scale);
    cr->set_source(map_pixels_, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_NEAREST);
    cr->paint();

    if (session_ != nullptr) {
        for (const auto &dot : session_->sprites()) {
            auto image = sprite_frame(dot.type, dot.frame);
            if (!image) {
                continue;
            }
            cr->save();
            cr->translate(dot.x + dot.x_offset, dot.y + dot.y_offset);
            cr->set_source(image, 0, 0);
            cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_NEAREST);
            cr->paint();
            cr->restore();
        }
    }
    cr->restore();
    draw_footprint(cr);
    return true;
}

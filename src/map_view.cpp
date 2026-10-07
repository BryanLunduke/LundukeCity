// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "map_view.hpp"

#include "sprite_art.hpp"
#include "tile_atlas.hpp"
#include "tools.hpp"
#include "view_math.hpp"
#include "zoom_keys.hpp"

#include "micropolis.h"

#include <gdk/gdk.h>
#include <gdkmm/cursor.h>
#include <glibmm/main.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/scrolledwindow.h>

#include <algorithm>
#include <cstdint>

namespace {

bool same_sprite(const CitySession::SpriteDot &a, const CitySession::SpriteDot &b)
{
    return a.type == b.type && a.frame == b.frame && a.x == b.x && a.y == b.y && a.x_offset == b.x_offset &&
           a.y_offset == b.y_offset && a.width == b.width && a.height == b.height;
}

} // namespace

MapView::MapView()
{
    set_can_focus(true);
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
               Gdk::BUTTON1_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK | Gdk::KEY_PRESS_MASK);
    set_tile_size(tile_size_);
    // Unpowered zones blink on their own timer so a paused city does not
    // need a full redraw just to toggle the lightning tile.
    blink_timer_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &MapView::on_blink), 500);
}

MapView::~MapView()
{
    blink_timer_.disconnect();
    edge_timer_.disconnect();
    ungrab_pointer();
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
    cached_raw_.clear();
    cached_shown_.clear();
    cached_sprites_.clear();
    queue_draw();
}

void MapView::set_tool(int engine_tool)
{
    if (engine_tool_ == engine_tool) {
        return;
    }
    const int previous = engine_tool_;
    if (hover_valid_) {
        invalidate_footprint(hover_x_, hover_y_, previous);
    }
    engine_tool_ = engine_tool;
    if (hover_valid_) {
        invalidate_footprint(hover_x_, hover_y_, engine_tool_);
    }
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
    const int old_x = hover_x_;
    const int old_y = hover_y_;
    const bool had = hover_valid_;
    hover_valid_ = true;
    hover_x_ = tx;
    hover_y_ = ty;
    if (had) {
        invalidate_footprint(old_x, old_y, engine_tool_);
    }
    invalidate_footprint(tx, ty, engine_tool_);
}

void MapView::clear_hover()
{
    if (!hover_valid_) {
        return;
    }
    const int old_x = hover_x_;
    const int old_y = hover_y_;
    hover_valid_ = false;
    invalidate_footprint(old_x, old_y, engine_tool_);
}

void MapView::set_tile_size(int pixels)
{
    tile_size_ = std::max(6, std::min(16, pixels));
    set_size_request(pixel_width(), pixel_height());
    queue_draw();
}

void MapView::set_shake(int dx, int dy)
{
    if (shake_x_ == dx && shake_y_ == dy) {
        return;
    }
    shake_x_ = dx;
    shake_y_ = dy;
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
    return map_tile_at(x, y, shake_x_, shake_y_, tile_size_, CitySession::kWorldW, CitySession::kWorldH, tx, ty);
}

void MapView::ensure_surface()
{
    const int width = CitySession::kWorldW * TileAtlas::kSize;
    const int height = CitySession::kWorldH * TileAtlas::kSize;
    if (!map_pixels_ || map_pixels_->get_width() != width || map_pixels_->get_height() != height) {
        map_pixels_ = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, height);
        cached_raw_.clear();
        cached_shown_.clear();
    }
    const std::size_t cells = static_cast<std::size_t>(CitySession::kWorldW * CitySession::kWorldH);
    if (cached_raw_.size() != cells) {
        cached_raw_.assign(cells, -1);
        cached_shown_.assign(cells, -1);
    }
}

int MapView::display_tile(int raw, const TileAtlas &atlas) const
{
    int tile = raw & LOMASK;
    if (blink_on_ && (raw & ZONEBIT) != 0 && (raw & PWRBIT) == 0 && atlas.loaded() &&
        LIGHTNINGBOLT < atlas.count()) {
        tile = LIGHTNINGBOLT;
    }
    return tile;
}

void MapView::blit_tile(int x, int y, int shown, const TileAtlas &atlas)
{
    unsigned char *data = map_pixels_->get_data();
    const int stride = map_pixels_->get_stride();
    unsigned char *dest = data + y * TileAtlas::kSize * stride + x * TileAtlas::kSize * 4;
    if (atlas.loaded()) {
        atlas.blit(shown, dest, stride);
        return;
    }
    const std::uint32_t dirt = (255u << 24) | (204u << 16) | (127u << 8) | 102u;
    for (int row = 0; row < TileAtlas::kSize; ++row) {
        auto *px = reinterpret_cast<std::uint32_t *>(dest + row * stride);
        for (int col = 0; col < TileAtlas::kSize; ++col) {
            px[col] = dirt;
        }
    }
}

void MapView::invalidate_tile(int x, int y)
{
    const int pad = 1;
    const int left = x * tile_size_ + shake_x_ - pad;
    const int top = y * tile_size_ + shake_y_ - pad;
    queue_draw_area(left, top, tile_size_ + pad * 2, tile_size_ + pad * 2);
}

void MapView::invalidate_footprint(int tx, int ty, int engine_tool)
{
    const ToolFootprint foot = tool_footprint(engine_tool);
    if (!foot.placeable || foot.width < 1 || foot.height < 1) {
        return;
    }
    const int pad = 4;
    const int left = (tx - foot.cursor_to_left) * tile_size_ + shake_x_ - pad;
    const int top = (ty - foot.cursor_to_top) * tile_size_ + shake_y_ - pad;
    const int width = foot.width * tile_size_ + pad * 2;
    const int height = foot.height * tile_size_ + pad * 2;
    queue_draw_area(left, top, width, height);
}

void MapView::invalidate_sprite(const CitySession::SpriteDot &dot)
{
    const double scale = static_cast<double>(tile_size_) / static_cast<double>(TileAtlas::kSize);
    const int left = shake_x_ + static_cast<int>((dot.x + dot.x_offset) * scale) - 2;
    const int top = shake_y_ + static_cast<int>((dot.y + dot.y_offset) * scale) - 2;
    const int width = std::max(tile_size_, static_cast<int>(dot.width * scale) + 4);
    const int height = std::max(tile_size_, static_cast<int>(dot.height * scale) + 4);
    queue_draw_area(left, top, width, height);
}

void MapView::sync()
{
    ensure_surface();
    TileAtlas &atlas = tile_atlas();
    constexpr int kSpotCap = 48;
    int spots[kSpotCap][2];
    int spot_count = 0;
    int dirty = 0;
    bool overflow = false;
    const int world_w = CitySession::kWorldW;
    const int world_h = CitySession::kWorldH;
    for (int y = 0; y < world_h; ++y) {
        for (int x = 0; x < world_w; ++x) {
            const int raw = session_ != nullptr ? session_->map_value(x, y) : 0;
            const int shown = display_tile(raw, atlas);
            const std::size_t index = static_cast<std::size_t>(y * world_w + x);
            if (cached_raw_[index] == raw && cached_shown_[index] == shown) {
                continue;
            }
            blit_tile(x, y, shown, atlas);
            cached_raw_[index] = raw;
            cached_shown_[index] = shown;
            ++dirty;
            if (!overflow) {
                if (spot_count < kSpotCap) {
                    spots[spot_count][0] = x;
                    spots[spot_count][1] = y;
                    ++spot_count;
                } else {
                    overflow = true;
                }
            }
        }
    }
    if (dirty > 0) {
        map_pixels_->mark_dirty();
        if (overflow || !get_realized()) {
            queue_draw();
        } else {
            for (int i = 0; i < spot_count; ++i) {
                invalidate_tile(spots[i][0], spots[i][1]);
            }
        }
    }

    std::vector<CitySession::SpriteDot> dots;
    if (session_ != nullptr) {
        dots = session_->sprites();
    }
    bool sprites_changed = dots.size() != cached_sprites_.size();
    if (!sprites_changed) {
        for (std::size_t i = 0; i < dots.size(); ++i) {
            if (!same_sprite(dots[i], cached_sprites_[i])) {
                sprites_changed = true;
                break;
            }
        }
    }
    if (!sprites_changed) {
        return;
    }
    for (const auto &dot : cached_sprites_) {
        invalidate_sprite(dot);
    }
    for (const auto &dot : dots) {
        invalidate_sprite(dot);
    }
    cached_sprites_ = std::move(dots);
}

bool MapView::on_blink()
{
    blink_on_ = !blink_on_;
    sync();
    return true;
}

void MapView::grab_pointer(GdkEventButton *event)
{
    auto window = get_window();
    if (!window) {
        return;
    }
    GdkDisplay *display = gdk_window_get_display(window->gobj());
    GdkSeat *seat = gdk_display_get_default_seat(display);
    if (seat == nullptr) {
        return;
    }
    GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_CROSSHAIR);
    const GdkGrabStatus status =
        gdk_seat_grab(seat, window->gobj(), GDK_SEAT_CAPABILITY_ALL_POINTING, FALSE, cursor,
                      reinterpret_cast<GdkEvent *>(event), nullptr, nullptr);
    if (cursor != nullptr) {
        g_object_unref(cursor);
    }
    pointer_grabbed_ = status == GDK_GRAB_SUCCESS;
}

void MapView::ungrab_pointer()
{
    if (!pointer_grabbed_) {
        return;
    }
    pointer_grabbed_ = false;
    auto window = get_window();
    if (!window) {
        return;
    }
    GdkSeat *seat = gdk_display_get_default_seat(gdk_window_get_display(window->gobj()));
    if (seat != nullptr) {
        gdk_seat_ungrab(seat);
    }
}

void MapView::end_drag()
{
    dragging_ = false;
    edge_timer_.disconnect();
    ungrab_pointer();
}

bool MapView::scroll_at_edge(double root_x, double root_y)
{
    Gtk::ScrolledWindow *scroller = nullptr;
    for (Gtk::Widget *widget = get_parent(); widget != nullptr; widget = widget->get_parent()) {
        scroller = dynamic_cast<Gtk::ScrolledWindow *>(widget);
        if (scroller != nullptr) {
            break;
        }
    }
    if (scroller == nullptr || !scroller->get_window()) {
        return false;
    }
    int origin_x = 0;
    int origin_y = 0;
    scroller->get_window()->get_origin(origin_x, origin_y);
    const int margin = 28;
    const double step = std::max(tile_size_, 8);
    double dx = 0;
    double dy = 0;
    const double right = origin_x + scroller->get_allocated_width();
    const double bottom = origin_y + scroller->get_allocated_height();
    if (root_x < origin_x + margin) {
        dx = -step;
    } else if (root_x > right - margin) {
        dx = step;
    }
    if (root_y < origin_y + margin) {
        dy = -step;
    } else if (root_y > bottom - margin) {
        dy = step;
    }
    if (dx == 0.0 && dy == 0.0) {
        return false;
    }
    auto clamp_to = [](const Glib::RefPtr<Gtk::Adjustment> &adjustment, double delta) {
        if (!adjustment) {
            return;
        }
        const double limit = std::max(adjustment->get_lower(), adjustment->get_upper() - adjustment->get_page_size());
        const double next = adjustment->get_value() + delta;
        adjustment->set_value(std::max(adjustment->get_lower(), std::min(limit, next)));
    };
    clamp_to(scroller->get_hadjustment(), dx);
    clamp_to(scroller->get_vadjustment(), dy);
    return true;
}

void MapView::follow_local_pointer(double x, double y, bool drag)
{
    int tx = 0;
    int ty = 0;
    if (!tile_at(x, y, tx, ty)) {
        clear_hover();
        return;
    }
    set_hover_tile(tx, ty);
    if (!drag || !dragging_) {
        return;
    }
    if (tx == last_x_ && ty == last_y_) {
        return;
    }
    signal_tool_drag.emit(last_x_, last_y_, tx, ty);
    last_x_ = tx;
    last_y_ = ty;
}

void MapView::follow_root_pointer(double root_x, double root_y, bool drag)
{
    auto window = get_window();
    if (!window) {
        return;
    }
    int origin_x = 0;
    int origin_y = 0;
    window->get_origin(origin_x, origin_y);
    follow_local_pointer(root_x - origin_x, root_y - origin_y, drag);
}

bool MapView::on_edge_scroll()
{
    if (!dragging_) {
        return false;
    }
    int root_x = static_cast<int>(last_root_x_);
    int root_y = static_cast<int>(last_root_y_);
    if (auto window = get_window()) {
        GdkSeat *seat = gdk_display_get_default_seat(gdk_window_get_display(window->gobj()));
        GdkDevice *pointer = seat != nullptr ? gdk_seat_get_pointer(seat) : nullptr;
        if (pointer != nullptr) {
            gdk_device_get_position(pointer, nullptr, &root_x, &root_y);
        }
    }
    scroll_at_edge(root_x, root_y);
    follow_root_pointer(root_x, root_y, true);
    return dragging_;
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
    last_root_x_ = event->x_root;
    last_root_y_ = event->y_root;
    // A click on the map leaves it focused, so the next Ctrl+= / Ctrl+-
    // is delivered here as well as through the window accelerator.
    grab_focus();
    grab_pointer(event);
    if (!edge_timer_.connected()) {
        edge_timer_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &MapView::on_edge_scroll), 50);
    }
    signal_tool_down.emit(tx, ty);
    return true;
}

bool MapView::on_button_release_event(GdkEventButton *event)
{
    if (event->button == 1) {
        end_drag();
    }
    return true;
}

bool MapView::on_motion_notify_event(GdkEventMotion *event)
{
    last_root_x_ = event->x_root;
    last_root_y_ = event->y_root;
    // A release outside the widget used to leave dragging_ set. With the
    // grab, that release is delivered here. If button 1 is already up,
    // drop the stroke instead of connecting it to the next tile.
    if (dragging_ && (event->state & GDK_BUTTON1_MASK) == 0) {
        end_drag();
    }
    const bool drag = dragging_ && (event->state & GDK_BUTTON1_MASK) != 0;
    follow_local_pointer(event->x, event->y, drag);
    return true;
}

bool MapView::on_leave_notify_event(GdkEventCrossing *)
{
    // A grabbed drag still receives motion outside the widget. Leaving
    // the allocation should not cancel that stroke.
    if (!dragging_) {
        clear_hover();
    }
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
    if (!map_pixels_) {
        sync();
    }
    if (!map_pixels_) {
        return true;
    }

    cr->save();
    cr->translate(shake_x_, shake_y_);
    const double scale = static_cast<double>(tile_size_) / static_cast<double>(TileAtlas::kSize);
    cr->save();
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
    // The footprint shares the earthquake translation with the tiles.
    // The scale above is only for the 16-pixel atlas.
    draw_footprint(cr);
    cr->restore();
    return true;
}

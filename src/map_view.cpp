// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "map_view.hpp"

#include "city_session.hpp"

#include "micropolis.h"

#include <algorithm>

namespace {

struct Rgb {
    double r, g, b;
};

void fill(const Cairo::RefPtr<Cairo::Context> &cr, int x, int y, int w, int h, Rgb c)
{
    cr->set_source_rgb(c.r, c.g, c.b);
    cr->rectangle(x, y, w, h);
    cr->fill();
}

bool is_water(int t)
{
    return t >= RIVER && t <= WATER_HIGH;
}

bool is_road(int t)
{
    return (t >= ROADBASE && t <= BRWXXX7) || t == ROADVPOWERH;
}

bool road_links(int t)
{
    return is_road(t) || t == HRAILROAD || t == VRAILROAD;
}

bool is_rail(int t)
{
    return (t >= RAILBASE && t <= LASTRAIL) || t == RAILHPOWERV || t == RAILVPOWERH;
}

bool is_wire(int t)
{
    return (t >= POWERBASE && t <= LASTPOWER) || t == HROADPOWER || t == VROADPOWER;
}

Rgb land_color()
{
    return {0.76, 0.47, 0.18};
}

Rgb water_color()
{
    return {0.12, 0.28, 0.82};
}

void paint_links(const Cairo::RefPtr<Cairo::Context> &cr, int px, int py, int s,
                 bool north, bool south, bool west, bool east, Rgb color, int band)
{
    const int off = (s - band) / 2;
    fill(cr, px + off, py + off, band, band, color);
    if (north) {
        fill(cr, px + off, py, band, off, color);
    }
    if (south) {
        fill(cr, px + off, py + off + band, band, s - off - band, color);
    }
    if (west) {
        fill(cr, px, py + off, off, band, color);
    }
    if (east) {
        fill(cr, px + off + band, py + off, s - off - band, band, color);
    }
}

void paint_building(const Cairo::RefPtr<Cairo::Context> &cr, int px, int py, int s,
                    Rgb wall, Rgb roof)
{
    const int m = std::max(1, s / 8);
    fill(cr, px + m, py + m, s - 2 * m, s - 2 * m, wall);
    fill(cr, px + m, py + m, s - 2 * m, std::max(1, s / 5), roof);
}

} // namespace

MapView::MapView()
{
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::POINTER_MOTION_MASK |
               Gdk::BUTTON1_MOTION_MASK);
    set_tile_size(tile_size_);
}

void MapView::set_session(CitySession *session)
{
    session_ = session;
    queue_draw();
}

void MapView::set_tool(int engine_tool)
{
    engine_tool_ = engine_tool;
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
    if (!dragging_ || (event->state & GDK_BUTTON1_MASK) == 0) {
        return false;
    }
    int tx = 0;
    int ty = 0;
    if (!tile_at(event->x, event->y, tx, ty)) {
        return false;
    }
    if (tx == last_x_ && ty == last_y_) {
        return true;
    }
    signal_tool_drag.emit(last_x_, last_y_, tx, ty);
    last_x_ = tx;
    last_y_ = ty;
    return true;
}

bool MapView::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    const int s = tile_size_;
    const Rgb dirt = land_color();
    const Rgb water = water_color();

    auto sample = [&](int x, int y) -> int {
        if (session_ == nullptr) {
            return DIRT;
        }
        return session_->map_value(x, y) & LOMASK;
    };

    for (int y = 0; y < CitySession::kWorldH; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int raw = session_ != nullptr ? session_->map_value(x, y) : DIRT;
            const int t = raw & LOMASK;
            const bool powered = (raw & PWRBIT) != 0;
            const int px = x * s;
            const int py = y * s;

            if (is_water(t) || t == FLOOD || (t > FLOOD && t <= LASTFLOOD)) {
                fill(cr, px, py, s, s, t == FLOOD || (t > FLOOD && t <= LASTFLOOD)
                                            ? Rgb{0.20, 0.45, 0.72}
                                            : water);
            } else if (t >= TREEBASE && t <= WOODS5) {
                fill(cr, px, py, s, s, dirt);
                fill(cr, px + s / 5, py + s / 5, std::max(2, s / 2), std::max(2, s / 2),
                     {0.12, 0.52, 0.16});
            } else if (t >= RUBBLE && t <= LASTRUBBLE) {
                fill(cr, px, py, s, s, {0.45, 0.42, 0.38});
            } else if (t == RADTILE) {
                fill(cr, px, py, s, s, {0.55, 0.75, 0.20});
            } else if (t >= FIREBASE && t <= LASTFIRE) {
                fill(cr, px, py, s, s, {0.85, 0.25, 0.08});
            } else if (t >= RESBASE && t < COMBASE) {
                fill(cr, px, py, s, s, {0.35, 0.62, 0.22});
                if (t >= HOUSE) {
                    paint_building(cr, px, py, s, {0.93, 0.93, 0.90}, {0.75, 0.22, 0.18});
                } else {
                    fill(cr, px + s / 3, py + s / 3, std::max(2, s / 3), std::max(2, s / 3),
                         {0.93, 0.93, 0.90});
                }
            } else if (t >= COMBASE && t < INDBASE) {
                fill(cr, px, py, s, s, dirt);
                paint_building(cr, px, py, s, {0.86, 0.88, 0.95}, {0.25, 0.38, 0.78});
            } else if (t >= INDBASE && t < PORTBASE) {
                fill(cr, px, py, s, s, dirt);
                paint_building(cr, px, py, s, {0.82, 0.74, 0.28}, {0.35, 0.32, 0.28});
            } else if (t >= PORTBASE && t < AIRPORTBASE) {
                fill(cr, px, py, s, s, {0.20, 0.40, 0.70});
                paint_building(cr, px, py, s, {0.75, 0.78, 0.82}, {0.30, 0.32, 0.36});
            } else if (t >= AIRPORTBASE && t < COALBASE) {
                fill(cr, px, py, s, s, {0.55, 0.55, 0.52});
                fill(cr, px, py + s / 2 - 1, s, std::max(1, s / 6), {0.92, 0.92, 0.92});
            } else if (t >= COALBASE && t <= LASTPOWERPLANT) {
                fill(cr, px, py, s, s, {0.30, 0.30, 0.32});
                fill(cr, px + s / 2, py, std::max(1, s / 5), s / 2, {0.15, 0.15, 0.16});
            } else if (t >= FIRESTBASE && t < POLICESTBASE) {
                fill(cr, px, py, s, s, dirt);
                paint_building(cr, px, py, s, {0.80, 0.18, 0.14}, {0.95, 0.85, 0.20});
            } else if (t >= POLICESTBASE && t < STADIUMBASE) {
                fill(cr, px, py, s, s, dirt);
                paint_building(cr, px, py, s, {0.20, 0.32, 0.72}, {0.90, 0.90, 0.95});
            } else if (t >= STADIUMBASE && t < NUCLEARBASE) {
                fill(cr, px, py, s, s, {0.25, 0.60, 0.28});
                fill(cr, px + 1, py + s / 4, s - 2, s / 2, {0.70, 0.70, 0.68});
            } else if (t >= NUCLEARBASE && t <= LASTZONE) {
                fill(cr, px, py, s, s, dirt);
                fill(cr, px + s / 6, py + s / 5, (s * 2) / 3, (s * 3) / 5, {0.55, 0.82, 0.78});
            } else if (is_road(t)) {
                fill(cr, px, py, s, s, dirt);
                const int band = std::max(2, s * 4 / 10);
                const bool n = y > 0 && road_links(sample(x, y - 1));
                const bool south = y + 1 < CitySession::kWorldH && road_links(sample(x, y + 1));
                const bool w = x > 0 && road_links(sample(x - 1, y));
                const bool e = x + 1 < CitySession::kWorldW && road_links(sample(x + 1, y));
                paint_links(cr, px, py, s, n, south, w, e, {0.42, 0.42, 0.42}, band);
                if (!n && !south && !w && !e) {
                    paint_links(cr, px, py, s, false, false, true, true, {0.42, 0.42, 0.42}, band);
                }
                const int mark = std::max(1, band / 3);
                fill(cr, px + (s - mark) / 2, py + (s - mark) / 2, mark, mark, {0.90, 0.90, 0.90});
            } else if (is_rail(t)) {
                fill(cr, px, py, s, s, dirt);
                const int band = std::max(2, s / 4);
                const bool n = y > 0 && is_rail(sample(x, y - 1));
                const bool south = y + 1 < CitySession::kWorldH && is_rail(sample(x, y + 1));
                const bool w = x > 0 && is_rail(sample(x - 1, y));
                const bool e = x + 1 < CitySession::kWorldW && is_rail(sample(x + 1, y));
                paint_links(cr, px, py, s, n, south, w, e, {0.22, 0.22, 0.24}, band);
                if (!n && !south && !w && !e) {
                    paint_links(cr, px, py, s, false, false, true, true, {0.22, 0.22, 0.24}, band);
                }
            } else if (is_wire(t)) {
                fill(cr, px, py, s, s, dirt);
                const int band = std::max(1, s / 6);
                const bool n = y > 0 && (is_wire(sample(x, y - 1)) || (session_ && (session_->map_value(x, y - 1) & PWRBIT)));
                const bool south = y + 1 < CitySession::kWorldH && is_wire(sample(x, y + 1));
                const bool w = x > 0 && is_wire(sample(x - 1, y));
                const bool e = x + 1 < CitySession::kWorldW && is_wire(sample(x + 1, y));
                paint_links(cr, px, py, s, n, south, w, e, {0.92, 0.82, 0.12}, band);
                if (!n && !south && !w && !e) {
                    paint_links(cr, px, py, s, false, false, true, true, {0.92, 0.82, 0.12}, band);
                }
            } else {
                fill(cr, px, py, s, s, dirt);
            }

            if (powered && s >= 8 && t >= RESBASE && t <= LASTZONE) {
                fill(cr, px + s - std::max(2, s / 5) - 1, py + 1, std::max(2, s / 5),
                     std::max(2, s / 5), {0.98, 0.86, 0.15});
            }
        }
    }

    if (session_ != nullptr) {
        for (const auto &dot : session_->sprites()) {
            if (dot.tile_x < 0 || dot.tile_y < 0 || dot.tile_x >= CitySession::kWorldW ||
                dot.tile_y >= CitySession::kWorldH) {
                continue;
            }
            Rgb color{0.95, 0.95, 0.95};
            switch (dot.type) {
            case SPRITE_TRAIN:
                color = {0.05, 0.05, 0.05};
                break;
            case SPRITE_HELICOPTER:
                color = {0.15, 0.70, 0.25};
                break;
            case SPRITE_AIRPLANE:
                color = {0.95, 0.95, 0.98};
                break;
            case SPRITE_SHIP:
                color = {0.05, 0.10, 0.30};
                break;
            case SPRITE_MONSTER:
                color = {0.55, 0.15, 0.70};
                break;
            case SPRITE_TORNADO:
                color = {0.75, 0.75, 0.78};
                break;
            case SPRITE_EXPLOSION:
                color = {0.95, 0.40, 0.05};
                break;
            case SPRITE_BUS:
                color = {0.90, 0.75, 0.10};
                break;
            default:
                break;
            }
            const int d = std::max(3, s / 2);
            fill(cr, dot.tile_x * s + (s - d) / 2, dot.tile_y * s + (s - d) / 2, d, d, color);
        }
    }

    return true;
}

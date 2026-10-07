// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "side_widgets.hpp"

#include "city_session.hpp"
#include "tile_atlas.hpp"

#include "micropolis.h"

#include <algorithm>
#include <cstdint>

namespace {

void fill(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h,
          double r, double g, double b)
{
    cr->set_source_rgb(r, g, b);
    cr->rectangle(x, y, w, h);
    cr->fill();
}

void tile_rgb(int raw, double &r, double &g, double &b)
{
    const TileAtlas &atlas = tile_atlas();
    if (atlas.loaded()) {
        atlas.average_color(raw & LOMASK, r, g, b);
        return;
    }
    const int t = raw & LOMASK;
    if (t >= RIVER && t <= WATER_HIGH) {
        r = 0.06;
        g = 0.24;
        b = 0.78;
    } else if (t >= TREEBASE && t <= WOODS5) {
        r = 0.05;
        g = 0.48;
        b = 0.10;
    } else if (t >= RESBASE && t < COMBASE) {
        r = 0.16;
        g = 0.62;
        b = 0.16;
    } else if (t >= COMBASE && t < INDBASE) {
        r = 0.28;
        g = 0.42;
        b = 0.86;
    } else if (t >= INDBASE && t < PORTBASE) {
        r = 0.92;
        g = 0.76;
        b = 0.08;
    } else if ((t >= ROADBASE && t <= BRWXXX7) || t == ROADVPOWERH) {
        r = 0.16;
        g = 0.16;
        b = 0.18;
    } else if (t >= RAILBASE && t <= LASTRAIL) {
        r = 0.45;
        g = 0.28;
        b = 0.10;
    } else if (t >= POWERBASE && t <= LASTPOWER) {
        r = 1.0;
        g = 0.92;
        b = 0.05;
    } else if (t >= FIREBASE && t <= LASTFIRE) {
        r = 0.92;
        g = 0.18;
        b = 0.06;
    } else {
        r = 0.86;
        g = 0.58;
        b = 0.26;
    }
}

std::uint32_t pack_rgb(double r, double g, double b)
{
    auto channel = [](double value) {
        const int byte = static_cast<int>(value * 255.0 + 0.5);
        return static_cast<std::uint32_t>(std::max(0, std::min(255, byte)));
    };
    return (channel(r) << 16) | (channel(g) << 8) | channel(b);
}

void paint_theme_background(Gtk::Widget &widget, const Cairo::RefPtr<Cairo::Context> &cr)
{
    widget.get_style_context()->render_background(cr, 0, 0, widget.get_allocated_width(),
                                                  widget.get_allocated_height());
}

} // namespace

MinimapView::MinimapView()
{
    // 78:65 is 6:5, the same shape as the 120x100 city, and it fits the
    // 80-pixel tool column.
    set_size_request(78, 65);
    set_hexpand(false);
    set_halign(Gtk::ALIGN_START);
    add_events(Gdk::BUTTON_PRESS_MASK);
}

void MinimapView::set_session(CitySession *session)
{
    session_ = session;
    cached_raw_.clear();
    queue_draw();
}

void MinimapView::set_viewport_provider(
    std::function<void(double &, double &, double &, double &)> provider)
{
    viewport_ = std::move(provider);
}

AspectBox MinimapView::land_box() const
{
    return largest_aspect_box(get_allocated_width(), get_allocated_height(), 6.0, 5.0);
}

void MinimapView::queue_viewport_rect(int x, int y, int width, int height)
{
    const int pad = 3;
    queue_draw_area(x - pad, y - pad, width + pad * 2, height + pad * 2);
}

void MinimapView::invalidate_viewport()
{
    if (!viewport_) {
        return;
    }
    double vx = 0;
    double vy = 0;
    double vw = 1;
    double vh = 1;
    viewport_(vx, vy, vw, vh);
    const AspectBox box = land_box();
    if (box.width < 1.0 || box.height < 1.0) {
        return;
    }
    const int x = static_cast<int>(box.x + vx * box.width);
    const int y = static_cast<int>(box.y + vy * box.height);
    const int w = std::max(4, static_cast<int>(vw * box.width));
    const int h = std::max(4, static_cast<int>(vh * box.height));
    if (viewport_valid_ && x == viewport_x_ && y == viewport_y_ && w == viewport_w_ && h == viewport_h_) {
        return;
    }
    if (viewport_valid_) {
        queue_viewport_rect(viewport_x_, viewport_y_, viewport_w_, viewport_h_);
    }
    queue_viewport_rect(x, y, w, h);
    viewport_x_ = x;
    viewport_y_ = y;
    viewport_w_ = w;
    viewport_h_ = h;
    viewport_valid_ = true;
}

void MinimapView::sync()
{
    if (!pixels_ || pixels_->get_width() != CitySession::kWorldW || pixels_->get_height() != CitySession::kWorldH) {
        pixels_ = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, CitySession::kWorldW, CitySession::kWorldH);
        cached_raw_.assign(static_cast<std::size_t>(CitySession::kWorldW * CitySession::kWorldH), -1);
    }
    const std::size_t cells = static_cast<std::size_t>(CitySession::kWorldW * CitySession::kWorldH);
    if (cached_raw_.size() != cells) {
        cached_raw_.assign(cells, -1);
    }

    unsigned char *data = pixels_->get_data();
    const int stride = pixels_->get_stride();
    constexpr int kSpotCap = 48;
    int spots[kSpotCap][2];
    int spot_count = 0;
    int dirty = 0;
    bool overflow = false;
    for (int y = 0; y < CitySession::kWorldH; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int raw = session_ != nullptr ? session_->map_value(x, y) : 0;
            const std::size_t index = static_cast<std::size_t>(y * CitySession::kWorldW + x);
            if (cached_raw_[index] == raw) {
                continue;
            }
            double r = 0;
            double g = 0;
            double b = 0;
            tile_rgb(raw, r, g, b);
            auto *pixel = reinterpret_cast<std::uint32_t *>(data + y * stride + x * 4);
            *pixel = pack_rgb(r, g, b);
            cached_raw_[index] = raw;
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
    if (dirty == 0) {
        return;
    }
    pixels_->mark_dirty();
    if (overflow || !get_realized()) {
        queue_draw();
        viewport_valid_ = false;
        return;
    }
    const AspectBox box = land_box();
    if (box.width < 1.0 || box.height < 1.0) {
        queue_draw();
        return;
    }
    for (int i = 0; i < spot_count; ++i) {
        const double sx = box.x + (spots[i][0] * box.width) / CitySession::kWorldW;
        const double sy = box.y + (spots[i][1] * box.height) / CitySession::kWorldH;
        const double sw = box.width / CitySession::kWorldW + 1.0;
        const double sh = box.height / CitySession::kWorldH + 1.0;
        queue_draw_area(static_cast<int>(sx), static_cast<int>(sy), static_cast<int>(sw) + 1,
                        static_cast<int>(sh) + 1);
    }
}

void MinimapView::on_size_allocate(Gtk::Allocation &allocation)
{
    Gtk::DrawingArea::on_size_allocate(allocation);
    viewport_valid_ = false;
    queue_draw();
}

bool MinimapView::on_button_press_event(GdkEventButton *event)
{
    if (event->button != 1) {
        return false;
    }
    const AspectBox box = land_box();
    if (box.width <= 1.0 || box.height <= 1.0) {
        return false;
    }
    double fx = 0;
    double fy = 0;
    aspect_box_fraction(box, event->x, event->y, fx, fy);
    signal_jump.emit(std::max(0.0, std::min(1.0, fx)), std::max(0.0, std::min(1.0, fy)));
    return true;
}

void MinimapView::paint_viewport(const Cairo::RefPtr<Cairo::Context> &cr) const
{
    if (!viewport_) {
        return;
    }
    double vx = 0;
    double vy = 0;
    double vw = 1;
    double vh = 1;
    viewport_(vx, vy, vw, vh);
    const AspectBox box = land_box();
    const double rx = box.x + vx * box.width;
    const double ry = box.y + vy * box.height;
    const double rw = std::max(4.0, vw * box.width);
    const double rh = std::max(4.0, vh * box.height);
    cr->set_line_width(2);
    cr->set_source_rgb(0, 0, 0);
    cr->rectangle(rx, ry, rw, rh);
    cr->stroke();
    cr->set_line_width(1);
    cr->set_source_rgb(1, 1, 1);
    cr->rectangle(rx + 1.5, ry + 1.5, std::max(1.0, rw - 3), std::max(1.0, rh - 3));
    cr->stroke();
}

bool MinimapView::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const AspectBox box = land_box();
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    paint_theme_background(*this, cr);
    if (box.width < 1.0 || box.height < 1.0) {
        return true;
    }
    if (!pixels_) {
        sync();
    }
    if (pixels_) {
        cr->save();
        cr->translate(box.x, box.y);
        cr->scale(box.width / CitySession::kWorldW, box.height / CitySession::kWorldH);
        cr->set_source(pixels_, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_NEAREST);
        cr->paint();
        cr->restore();
    } else {
        fill(cr, box.x, box.y, box.width, box.height, 0.86, 0.58, 0.26);
    }
    paint_viewport(cr);
    return true;
}

DemandView::DemandView()
{
    set_size_request(80, 84);
    set_hexpand(false);
    set_halign(Gtk::ALIGN_START);
}

void DemandView::set_session(CitySession *session)
{
    session_ = session;
    queue_draw();
}

bool DemandView::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const double w = get_allocated_width();
    const double h = get_allocated_height();
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    paint_theme_background(*this, cr);

    const double demands[3] = {
        session_ != nullptr ? session_->res_demand() : 0,
        session_ != nullptr ? session_->com_demand() : 0,
        session_ != nullptr ? session_->ind_demand() : 0,
    };
    const double colors[3][3] = {
        {0.05, 0.62, 0.12},
        {0.12, 0.28, 0.90},
        {0.95, 0.78, 0.02},
    };
    const char *letters[3] = {"R", "C", "I"};

    const double gap = 5;
    const double col_w = (w - gap * 4) / 3.0;
    const double top = 6;
    const double meter_h = h - 24;
    const double mid = top + meter_h / 2.0;

    for (int i = 0; i < 3; ++i) {
        const double x = gap + i * (col_w + gap);
        fill(cr, x, top, col_w, meter_h, 0.96, 0.96, 0.96);
        cr->set_source_rgb(0.15, 0.15, 0.15);
        cr->set_line_width(1);
        cr->rectangle(x + 0.5, top + 0.5, col_w - 1, meter_h - 1);
        cr->stroke();
        const double frac = demands[i];
        if (frac >= 0) {
            const double bh = (meter_h / 2.0 - 2) * std::min(1.0, frac);
            fill(cr, x + 2, mid - bh, col_w - 4, bh, colors[i][0], colors[i][1], colors[i][2]);
        } else {
            const double bh = (meter_h / 2.0 - 2) * std::min(1.0, -frac);
            fill(cr, x + 2, mid, col_w - 4, bh, colors[i][0], colors[i][1], colors[i][2]);
        }
        cr->set_source_rgb(0.05, 0.05, 0.05);
        cr->set_line_width(1);
        cr->move_to(x + 1, mid);
        cr->line_to(x + col_w - 1, mid);
        cr->stroke();

        const double text_r = i == 2 ? 0.45 : colors[i][0] * 0.65;
        const double text_g = i == 2 ? 0.32 : colors[i][1] * 0.75;
        const double text_b = i == 2 ? 0.0 : colors[i][2] * 0.75;
        cr->set_source_rgb(text_r, text_g, text_b);
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(13);
        Cairo::TextExtents ext;
        cr->get_text_extents(letters[i], ext);
        cr->move_to(x + (col_w - ext.width) / 2.0, h - 5);
        cr->show_text(letters[i]);
    }
    return true;
}

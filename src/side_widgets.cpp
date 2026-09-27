// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "side_widgets.hpp"

#include "city_session.hpp"

#include "micropolis.h"

#include <algorithm>

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

void inset_frame(const Cairo::RefPtr<Cairo::Context> &cr, double w, double h)
{
    cr->set_line_width(1);
    cr->set_source_rgb(0.25, 0.25, 0.25);
    cr->move_to(0.5, h - 0.5);
    cr->line_to(0.5, 0.5);
    cr->line_to(w - 0.5, 0.5);
    cr->stroke();
    cr->set_source_rgb(0.98, 0.98, 0.98);
    cr->move_to(0.5, h - 0.5);
    cr->line_to(w - 0.5, h - 0.5);
    cr->line_to(w - 0.5, 0.5);
    cr->stroke();
}

} // namespace

MinimapView::MinimapView()
{
    set_size_request(80, 78);
    set_hexpand(false);
    set_halign(Gtk::ALIGN_START);
    add_events(Gdk::BUTTON_PRESS_MASK);
}

void MinimapView::set_session(CitySession *session)
{
    session_ = session;
    queue_draw();
}

void MinimapView::set_viewport_provider(
    std::function<void(double &, double &, double &, double &)> provider)
{
    viewport_ = std::move(provider);
}

bool MinimapView::on_button_press_event(GdkEventButton *event)
{
    if (event->button != 1) {
        return false;
    }
    const int w = get_allocated_width();
    const int h = get_allocated_height();
    if (w <= 4 || h <= 4) {
        return false;
    }
    const double fx = (event->x - 2.0) / static_cast<double>(w - 4);
    const double fy = (event->y - 2.0) / static_cast<double>(h - 4);
    signal_jump.emit(std::max(0.0, std::min(1.0, fx)), std::max(0.0, std::min(1.0, fy)));
    return true;
}

bool MinimapView::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const double w = get_allocated_width();
    const double h = get_allocated_height();
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    fill(cr, 0, 0, w, h, 0.753, 0.753, 0.753);
    inset_frame(cr, w, h);
    const double inner_w = std::max(1.0, w - 6);
    const double inner_h = std::max(1.0, h - 6);
    fill(cr, 3, 3, inner_w, inner_h, 0.86, 0.58, 0.26);

    if (session_ != nullptr) {
        auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, CitySession::kWorldW,
                                                   CitySession::kWorldH);
        auto pic = Cairo::Context::create(surface);
        pic->set_antialias(Cairo::ANTIALIAS_NONE);
        for (int y = 0; y < CitySession::kWorldH; ++y) {
            for (int x = 0; x < CitySession::kWorldW; ++x) {
                double r, g, b;
                tile_rgb(session_->map_value(x, y), r, g, b);
                fill(pic, x, y, 1, 1, r, g, b);
            }
        }
        surface->flush();
        cr->save();
        cr->translate(3, 3);
        cr->scale(inner_w / CitySession::kWorldW, inner_h / CitySession::kWorldH);
        cr->set_source(surface, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_NEAREST);
        cr->paint();
        cr->restore();
    }

    if (viewport_) {
        double vx = 0, vy = 0, vw = 1, vh = 1;
        viewport_(vx, vy, vw, vh);
        const double rx = 3 + vx * inner_w;
        const double ry = 3 + vy * inner_h;
        const double rw = std::max(4.0, vw * inner_w);
        const double rh = std::max(4.0, vh * inner_h);
        cr->set_line_width(2);
        cr->set_source_rgb(0, 0, 0);
        cr->rectangle(rx, ry, rw, rh);
        cr->stroke();
        cr->set_line_width(1);
        cr->set_source_rgb(1, 1, 1);
        cr->rectangle(rx + 1.5, ry + 1.5, std::max(1.0, rw - 3), std::max(1.0, rh - 3));
        cr->stroke();
    }
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
    fill(cr, 0, 0, w, h, 0.753, 0.753, 0.753);
    inset_frame(cr, w, h);

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

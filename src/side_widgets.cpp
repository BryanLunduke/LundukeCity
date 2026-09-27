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
        r = 0.12;
        g = 0.28;
        b = 0.82;
    } else if (t >= TREEBASE && t <= WOODS5) {
        r = 0.13;
        g = 0.52;
        b = 0.16;
    } else if (t >= RESBASE && t < COMBASE) {
        r = 0.20;
        g = 0.62;
        b = 0.22;
    } else if (t >= COMBASE && t < INDBASE) {
        r = 0.25;
        g = 0.38;
        b = 0.82;
    } else if (t >= INDBASE && t < PORTBASE) {
        r = 0.78;
        g = 0.70;
        b = 0.18;
    } else if ((t >= ROADBASE && t <= BRWXXX7) || t == ROADVPOWERH) {
        r = 0.35;
        g = 0.35;
        b = 0.35;
    } else if (t >= RAILBASE && t <= LASTRAIL) {
        r = 0.20;
        g = 0.20;
        b = 0.22;
    } else if (t >= POWERBASE && t <= LASTPOWER) {
        r = 0.90;
        g = 0.80;
        b = 0.12;
    } else if (t >= FIREBASE && t <= LASTFIRE) {
        r = 0.90;
        g = 0.25;
        b = 0.08;
    } else {
        r = 0.76;
        g = 0.47;
        b = 0.18;
    }
}

} // namespace

MinimapView::MinimapView()
{
    set_size_request(76, 66);
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
    fill(cr, 0, 0, w, h, 0.15, 0.15, 0.15);
    const double inner_w = std::max(1.0, w - 4);
    const double inner_h = std::max(1.0, h - 4);
    fill(cr, 2, 2, inner_w, inner_h, 0.76, 0.47, 0.18);

    if (session_ != nullptr) {
        const double tw = inner_w / CitySession::kWorldW;
        const double th = inner_h / CitySession::kWorldH;
        for (int y = 0; y < CitySession::kWorldH; ++y) {
            for (int x = 0; x < CitySession::kWorldW; ++x) {
                double r, g, b;
                tile_rgb(session_->map_value(x, y), r, g, b);
                fill(cr, 2 + x * tw, 2 + y * th, std::max(1.0, tw), std::max(1.0, th), r, g, b);
            }
        }
    }

    if (viewport_) {
        double vx = 0, vy = 0, vw = 1, vh = 1;
        viewport_(vx, vy, vw, vh);
        cr->set_source_rgb(1, 1, 1);
        cr->set_line_width(1);
        cr->rectangle(2 + vx * inner_w, 2 + vy * inner_h, std::max(2.0, vw * inner_w),
                      std::max(2.0, vh * inner_h));
        cr->stroke();
    }
    return true;
}

DemandView::DemandView()
{
    set_size_request(76, 58);
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
    fill(cr, 0, 0, w, h, 0.82, 0.82, 0.82);

    const double demands[3] = {
        session_ != nullptr ? session_->res_demand() : 0,
        session_ != nullptr ? session_->com_demand() : 0,
        session_ != nullptr ? session_->ind_demand() : 0,
    };
    const double colors[3][3] = {
        {0.15, 0.65, 0.20},
        {0.20, 0.35, 0.85},
        {0.85, 0.75, 0.10},
    };
    const char *letters[3] = {"R", "C", "I"};

    const double gap = 6;
    const double col_w = (w - gap * 4) / 3.0;
    const double top = 4;
    const double meter_h = h - 18;
    const double mid = top + meter_h / 2.0;

    for (int i = 0; i < 3; ++i) {
        const double x = gap + i * (col_w + gap);
        fill(cr, x, top, col_w, meter_h, 0.25, 0.25, 0.25);
        const double frac = demands[i];
        if (frac >= 0) {
            const double bh = (meter_h / 2.0) * std::min(1.0, frac);
            fill(cr, x + 2, mid - bh, col_w - 4, bh, colors[i][0], colors[i][1], colors[i][2]);
        } else {
            const double bh = (meter_h / 2.0) * std::min(1.0, -frac);
            fill(cr, x + 2, mid, col_w - 4, bh, colors[i][0], colors[i][1], colors[i][2]);
        }
        cr->set_source_rgb(0.95, 0.95, 0.95);
        cr->move_to(x, mid);
        cr->line_to(x + col_w, mid);
        cr->stroke();

        cr->set_source_rgb(colors[i][0], colors[i][1], colors[i][2]);
        cr->select_font_face("Sans", Cairo::FONT_SLANT_NORMAL, Cairo::FONT_WEIGHT_BOLD);
        cr->set_font_size(11);
        Cairo::TextExtents ext;
        cr->get_text_extents(letters[i], ext);
        cr->move_to(x + (col_w - ext.width) / 2.0, h - 3);
        cr->show_text(letters[i]);
    }
    return true;
}

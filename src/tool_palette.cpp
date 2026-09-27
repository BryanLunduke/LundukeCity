// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "tool_palette.hpp"

#include "tools.hpp"

#include "micropolis.h"

#include <gdkmm/cursor.h>
#include <gtkmm/tooltip.h>

#include <algorithm>

namespace {

void box(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double w, double h,
         double r, double g, double b)
{
    cr->set_source_rgb(r, g, b);
    cr->rectangle(x, y, w, h);
    cr->fill();
}

void icon_bulldozer(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.15, y + s * 0.35, s * 0.55, s * 0.28, 0.95, 0.78, 0.15);
    box(cr, x + s * 0.40, y + s * 0.18, s * 0.28, s * 0.22, 0.15, 0.15, 0.15);
    box(cr, x + s * 0.08, y + s * 0.58, s * 0.18, s * 0.22, 0.35, 0.35, 0.35);
    box(cr, x + s * 0.62, y + s * 0.55, s * 0.22, s * 0.28, 0.75, 0.75, 0.78);
}

void icon_road(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.28, y + s * 0.12, s * 0.44, s * 0.76, 0.40, 0.40, 0.40);
    box(cr, x + s * 0.46, y + s * 0.18, s * 0.08, s * 0.16, 0.92, 0.92, 0.92);
    box(cr, x + s * 0.46, y + s * 0.42, s * 0.08, s * 0.16, 0.92, 0.92, 0.92);
    box(cr, x + s * 0.46, y + s * 0.66, s * 0.08, s * 0.16, 0.92, 0.92, 0.92);
}

void icon_rail(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.22, y + s * 0.18, s * 0.56, s * 0.10, 0.45, 0.28, 0.12);
    box(cr, x + s * 0.22, y + s * 0.45, s * 0.56, s * 0.10, 0.45, 0.28, 0.12);
    box(cr, x + s * 0.22, y + s * 0.72, s * 0.56, s * 0.10, 0.45, 0.28, 0.12);
    box(cr, x + s * 0.32, y + s * 0.12, s * 0.08, s * 0.76, 0.25, 0.25, 0.28);
    box(cr, x + s * 0.60, y + s * 0.12, s * 0.08, s * 0.76, 0.25, 0.25, 0.28);
}

void icon_wire(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.46, y + s * 0.18, s * 0.08, s * 0.62, 0.35, 0.22, 0.10);
    box(cr, x + s * 0.18, y + s * 0.28, s * 0.64, s * 0.06, 0.95, 0.82, 0.10);
    box(cr, x + s * 0.62, y + s * 0.40, s * 0.16, s * 0.10, 0.95, 0.82, 0.10);
    box(cr, x + s * 0.68, y + s * 0.50, s * 0.10, s * 0.16, 0.95, 0.82, 0.10);
}

void icon_park(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.44, y + s * 0.55, s * 0.12, s * 0.28, 0.40, 0.24, 0.10);
    box(cr, x + s * 0.22, y + s * 0.28, s * 0.56, s * 0.32, 0.12, 0.55, 0.16);
    box(cr, x + s * 0.32, y + s * 0.14, s * 0.36, s * 0.22, 0.16, 0.62, 0.20);
}

void icon_house(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.22, y + s * 0.42, s * 0.56, s * 0.40, 0.93, 0.93, 0.90);
    box(cr, x + s * 0.16, y + s * 0.28, s * 0.68, s * 0.16, 0.75, 0.18, 0.14);
    box(cr, x + s * 0.42, y + s * 0.58, s * 0.16, s * 0.24, 0.35, 0.22, 0.12);
}

void icon_shop(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.16, y + s * 0.30, s * 0.68, s * 0.52, 0.25, 0.38, 0.78);
    box(cr, x + s * 0.28, y + s * 0.48, s * 0.14, s * 0.18, 0.92, 0.92, 0.70);
    box(cr, x + s * 0.52, y + s * 0.48, s * 0.14, s * 0.18, 0.92, 0.92, 0.70);
}

void icon_factory(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.12, y + s * 0.48, s * 0.70, s * 0.34, 0.55, 0.50, 0.22);
    box(cr, x + s * 0.62, y + s * 0.16, s * 0.14, s * 0.36, 0.30, 0.30, 0.32);
    box(cr, x + s * 0.66, y + s * 0.08, s * 0.16, s * 0.10, 0.75, 0.75, 0.78);
}

void icon_police(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.28, y + s * 0.18, s * 0.44, s * 0.16, 0.15, 0.25, 0.70);
    box(cr, x + s * 0.22, y + s * 0.34, s * 0.56, s * 0.42, 0.20, 0.36, 0.78);
    box(cr, x + s * 0.44, y + s * 0.42, s * 0.12, s * 0.22, 0.95, 0.85, 0.20);
}

void icon_fire(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.18, y + s * 0.48, s * 0.64, s * 0.34, 0.75, 0.16, 0.12);
    box(cr, x + s * 0.40, y + s * 0.16, s * 0.20, s * 0.34, 0.95, 0.55, 0.10);
    box(cr, x + s * 0.46, y + s * 0.08, s * 0.10, s * 0.16, 0.98, 0.85, 0.25);
}

void icon_stadium(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.12, y + s * 0.28, s * 0.76, s * 0.44, 0.55, 0.55, 0.52);
    box(cr, x + s * 0.24, y + s * 0.38, s * 0.52, s * 0.24, 0.20, 0.62, 0.24);
}

void icon_anchor(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.44, y + s * 0.16, s * 0.12, s * 0.58, 0.12, 0.12, 0.16);
    box(cr, x + s * 0.22, y + s * 0.62, s * 0.56, s * 0.10, 0.12, 0.12, 0.16);
    box(cr, x + s * 0.22, y + s * 0.40, s * 0.10, s * 0.32, 0.12, 0.12, 0.16);
    box(cr, x + s * 0.68, y + s * 0.40, s * 0.10, s * 0.32, 0.12, 0.12, 0.16);
    box(cr, x + s * 0.40, y + s * 0.08, s * 0.20, s * 0.12, 0.12, 0.12, 0.16);
}

void icon_coal(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.14, y + s * 0.46, s * 0.58, s * 0.36, 0.28, 0.28, 0.30);
    box(cr, x + s * 0.62, y + s * 0.18, s * 0.16, s * 0.40, 0.18, 0.18, 0.20);
    box(cr, x + s * 0.66, y + s * 0.10, s * 0.14, s * 0.10, 0.95, 0.80, 0.15);
}

void icon_nuclear(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.30, y + s * 0.48, s * 0.40, s * 0.28, 0.35, 0.55, 0.58);
    box(cr, x + s * 0.22, y + s * 0.28, s * 0.56, s * 0.24, 0.55, 0.82, 0.78);
    box(cr, x + s * 0.46, y + s * 0.14, s * 0.08, s * 0.18, 0.20, 0.35, 0.38);
}

void icon_airport(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.10, y + s * 0.46, s * 0.80, s * 0.10, 0.45, 0.45, 0.42);
    box(cr, x + s * 0.38, y + s * 0.22, s * 0.24, s * 0.56, 0.92, 0.92, 0.94);
    box(cr, x + s * 0.22, y + s * 0.40, s * 0.56, s * 0.12, 0.80, 0.82, 0.86);
}

void icon_query(const Cairo::RefPtr<Cairo::Context> &cr, double x, double y, double s)
{
    box(cr, x + s * 0.30, y + s * 0.12, s * 0.36, s * 0.12, 0.10, 0.10, 0.12);
    box(cr, x + s * 0.58, y + s * 0.20, s * 0.12, s * 0.22, 0.10, 0.10, 0.12);
    box(cr, x + s * 0.42, y + s * 0.40, s * 0.16, s * 0.18, 0.10, 0.10, 0.12);
    box(cr, x + s * 0.44, y + s * 0.66, s * 0.14, s * 0.14, 0.10, 0.10, 0.12);
}

using IconFn = void (*)(const Cairo::RefPtr<Cairo::Context> &, double, double, double);

IconFn icon_for(int engine_id)
{
    switch (engine_id) {
    case TOOL_BULLDOZER:
        return icon_bulldozer;
    case TOOL_ROAD:
        return icon_road;
    case TOOL_RAILROAD:
        return icon_rail;
    case TOOL_WIRE:
        return icon_wire;
    case TOOL_PARK:
        return icon_park;
    case TOOL_RESIDENTIAL:
        return icon_house;
    case TOOL_COMMERCIAL:
        return icon_shop;
    case TOOL_INDUSTRIAL:
        return icon_factory;
    case TOOL_POLICESTATION:
        return icon_police;
    case TOOL_FIRESTATION:
        return icon_fire;
    case TOOL_STADIUM:
        return icon_stadium;
    case TOOL_SEAPORT:
        return icon_anchor;
    case TOOL_COALPOWER:
        return icon_coal;
    case TOOL_NUCLEARPOWER:
        return icon_nuclear;
    case TOOL_AIRPORT:
        return icon_airport;
    case TOOL_QUERY:
        return icon_query;
    default:
        return icon_query;
    }
}

} // namespace

ToolPalette::ToolPalette()
{
    set_size_request(kPad * 2 + kCols * kCell, kPad * 2 + (kToolCount / kCols) * kCell);
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK);
    set_has_tooltip(true);
    signal_query_tooltip().connect(sigc::mem_fun(*this, &ToolPalette::on_query_tooltip));
    selected_ = kDefaultToolIndex;
}

void ToolPalette::on_realize()
{
    Gtk::DrawingArea::on_realize();
    if (auto window = get_window()) {
        window->set_cursor(Gdk::Cursor::create(get_display(), Gdk::HAND2));
    }
}

void ToolPalette::set_selected(int index)
{
    if (index < 0 || index >= kToolCount || index == selected_) {
        return;
    }
    selected_ = index;
    queue_draw();
    signal_selected.emit(selected_);
}

int ToolPalette::index_at(double x, double y) const
{
    if (x < kPad || y < kPad) {
        return -1;
    }
    const int col = static_cast<int>(x - kPad) / kCell;
    const int row = static_cast<int>(y - kPad) / kCell;
    if (col < 0 || row < 0 || col >= kCols) {
        return -1;
    }
    const int index = row * kCols + col;
    if (index < 0 || index >= kToolCount) {
        return -1;
    }
    return index;
}

bool ToolPalette::on_button_press_event(GdkEventButton *event)
{
    if (event->button != 1) {
        return false;
    }
    const int index = index_at(event->x, event->y);
    if (index < 0) {
        return false;
    }
    if (index != selected_) {
        selected_ = index;
        queue_draw();
        signal_selected.emit(selected_);
    }
    return true;
}

bool ToolPalette::on_motion_notify_event(GdkEventMotion *event)
{
    const int index = index_at(event->x, event->y);
    if (index != hover_) {
        hover_ = index;
        queue_draw();
    }
    return true;
}

bool ToolPalette::on_leave_notify_event(GdkEventCrossing *)
{
    if (hover_ != -1) {
        hover_ = -1;
        queue_draw();
    }
    return true;
}

bool ToolPalette::on_query_tooltip(int x, int y, bool, const Glib::RefPtr<Gtk::Tooltip> &tooltip)
{
    const int index = index_at(x, y);
    if (index < 0) {
        return false;
    }
    tooltip->set_text(tool_by_index(index)->name);
    return true;
}

bool ToolPalette::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const int rows = kToolCount / kCols;
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    box(cr, 0, 0, kPad * 2 + kCols * kCell, kPad * 2 + rows * kCell, 0.753, 0.753, 0.753);

    for (int i = 0; i < kToolCount; ++i) {
        const int col = i % kCols;
        const int row = i / kCols;
        const double x = kPad + col * kCell + 1;
        const double y = kPad + row * kCell + 1;
        const double bs = kCell - 2;
        const bool pressed = i == selected_;
        const bool hover = i == hover_ && !pressed;

        const double light = hover ? 1.0 : 0.96;
        const double dark = 0.28;
        const double face_r = pressed ? 1.0 : (hover ? 0.93 : 0.82);
        const double face_g = pressed ? 0.86 : (hover ? 0.93 : 0.82);
        const double face_b = pressed ? 0.15 : (hover ? 0.93 : 0.82);

        if (pressed) {
            box(cr, x, y, bs, bs, dark, dark, dark);
            box(cr, x + 2, y + bs - 2, bs - 2, 2, light, light, light);
            box(cr, x + bs - 2, y + 2, 2, bs - 4, light, light, light);
            box(cr, x + 2, y + 2, bs - 4, bs - 4, face_r, face_g, face_b);
            cr->set_source_rgb(0.05, 0.05, 0.05);
            cr->set_line_width(1);
            cr->rectangle(x + 2.5, y + 2.5, bs - 6, bs - 6);
            cr->stroke();
        } else {
            box(cr, x, y, bs, bs, light, light, light);
            box(cr, x, y + bs - 2, bs, 2, dark, dark, dark);
            box(cr, x + bs - 2, y, 2, bs, dark, dark, dark);
            box(cr, x + 1, y + bs - 3, bs - 3, 1, 0.50, 0.50, 0.50);
            box(cr, x + bs - 3, y + 1, 1, bs - 3, 0.50, 0.50, 0.50);
            box(cr, x + 2, y + 2, bs - 5, bs - 5, face_r, face_g, face_b);
        }

        const double shift = pressed ? 1.0 : 0.0;
        icon_for(kTools[i].engine_id)(cr, x + 4 + shift, y + 4 + shift, bs - 10);
    }
    return true;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "overlay_window.hpp"

#include <algorithm>
#include <cmath>

namespace {

struct Rgb {
    double r, g, b;
};

const char *layer_title(CitySession::MapLayer layer)
{
    switch (layer) {
    case CitySession::MapLayer::Power:
        return "Power";
    case CitySession::MapLayer::Water:
        return "Water";
    case CitySession::MapLayer::Pollution:
        return "Pollution";
    case CitySession::MapLayer::Crime:
        return "Crime";
    case CitySession::MapLayer::LandValue:
        return "Land value";
    case CitySession::MapLayer::Traffic:
        return "Traffic";
    }
    return "Map";
}

const char *layer_legend(CitySession::MapLayer layer)
{
    switch (layer) {
    case CitySession::MapLayer::Power:
        return "Yellow powered zones, red unpowered zones, orange lines, blue water.";
    case CitySession::MapLayer::Water:
        return "Blue is water and flood. The engine stores water as map tiles.";
    case CitySession::MapLayer::Pollution:
        return "Brighter gray means higher pollution on the engine's density map.";
    case CitySession::MapLayer::Crime:
        return "Brighter red means a higher crime rate on the engine's crime map.";
    case CitySession::MapLayer::LandValue:
        return "Brighter green means higher land value on the engine's land-value map.";
    case CitySession::MapLayer::Traffic:
        return "Brighter orange means heavier traffic on the engine's traffic map.";
    }
    return "";
}

Rgb heat(CitySession::MapLayer layer, double t)
{
    t = std::max(0.0, std::min(1.0, t));
    switch (layer) {
    case CitySession::MapLayer::Pollution:
        return {0.15 + 0.75 * t, 0.16 + 0.16 * t, 0.14 + 0.10 * t};
    case CitySession::MapLayer::Crime:
        return {0.20 + 0.75 * t, 0.05, 0.12 + 0.15 * t};
    case CitySession::MapLayer::LandValue:
        return {0.05 + 0.25 * t, 0.18 + 0.62 * t, 0.08 + 0.15 * t};
    case CitySession::MapLayer::Traffic:
        return {0.25 + 0.70 * t, 0.12 + 0.35 * t, 0.05};
    default:
        return {t, t, t};
    }
}

} // namespace

OverlayWindow::OverlayWindow(CitySession::MapLayer layer)
    : layer_(layer)
{
    set_title(layer_title(layer));
    set_border_width(8);
    signal_delete_event().connect([this](GdkEventAny *) {
        hide();
        return true;
    });

    legend_.set_text(layer_legend(layer));
    legend_.set_halign(Gtk::ALIGN_START);
    legend_.set_line_wrap(true);
    legend_.set_max_width_chars(48);

    map_.set_size_request(CitySession::kWorldW * 4, CitySession::kWorldH * 4);
    map_.signal_draw().connect(sigc::mem_fun(*this, &OverlayWindow::on_draw_map));

    root_.pack_start(legend_, Gtk::PACK_SHRINK);
    root_.pack_start(map_, Gtk::PACK_SHRINK);
    add(root_);
}

void OverlayWindow::set_session(CitySession *session)
{
    session_ = session;
}

void OverlayWindow::present_map()
{
    show_all();
    present();
    map_.queue_draw();
}

bool OverlayWindow::on_draw_map(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const int cells_w = CitySession::kWorldW;
    const int cells_h = CitySession::kWorldH;
    const double w = std::max(1, map_.get_allocated_width());
    const double h = std::max(1, map_.get_allocated_height());
    const double cw = w / cells_w;
    const double ch = h / cells_h;

    int peak = 1;
    if (session_ != nullptr && layer_ != CitySession::MapLayer::Power &&
        layer_ != CitySession::MapLayer::Water) {
        for (int y = 0; y < cells_h; y += 2) {
            for (int x = 0; x < cells_w; x += 2) {
                peak = std::max(peak, session_->layer_value(layer_, x, y));
            }
        }
    }

    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    for (int y = 0; y < cells_h; ++y) {
        for (int x = 0; x < cells_w; ++x) {
            const int water = session_ != nullptr ? session_->layer_value(CitySession::MapLayer::Water, x, y) : 0;
            Rgb color{0.55, 0.42, 0.28};
            if (water > 0) {
                color = {0.10, 0.28, 0.72};
            }
            if (session_ != nullptr && layer_ == CitySession::MapLayer::Power && water == 0) {
                switch (session_->layer_value(layer_, x, y)) {
                case 2:
                    color = {0.85, 0.12, 0.10};
                    break;
                case 3:
                    color = {0.98, 0.86, 0.15};
                    break;
                case 4:
                    color = {0.95, 0.55, 0.10};
                    break;
                default:
                    break;
                }
            } else if (session_ != nullptr && layer_ == CitySession::MapLayer::Water) {
                color = water > 0 ? Rgb{0.15, 0.45, 0.95} : Rgb{0.45, 0.36, 0.24};
            } else if (session_ != nullptr && water == 0 && layer_ != CitySession::MapLayer::Power) {
                const double t = session_->layer_value(layer_, x, y) / static_cast<double>(peak);
                if (t > 0) {
                    color = heat(layer_, t);
                }
            }
            cr->set_source_rgb(color.r, color.g, color.b);
            cr->rectangle(x * cw, y * ch, cw + 0.5, ch + 0.5);
            cr->fill();
        }
    }
    return true;
}

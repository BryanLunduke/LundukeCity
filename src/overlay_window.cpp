// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "overlay_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
    map_.add_events(Gdk::BUTTON_PRESS_MASK);
    map_.signal_draw().connect(sigc::mem_fun(*this, &OverlayWindow::on_draw_map));
    map_.signal_button_press_event().connect(sigc::mem_fun(*this, &OverlayWindow::on_map_button));

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
    cells_.clear();
    sync();
}

bool OverlayWindow::density_layer() const
{
    return layer_ != CitySession::MapLayer::Power && layer_ != CitySession::MapLayer::Water;
}

void OverlayWindow::sync()
{
    if (session_ == nullptr || !get_visible()) {
        return;
    }
    const bool density = density_layer();
    const int cells_w = density ? CitySession::kWorldW / 2 : CitySession::kWorldW;
    const int cells_h = density ? CitySession::kWorldH / 2 : CitySession::kWorldH;
    const int count = cells_w * cells_h;
    std::vector<int> next(static_cast<std::size_t>(count));
    int peak = 1;
    for (int y = 0; y < cells_h; ++y) {
        for (int x = 0; x < cells_w; ++x) {
            const int tile_x = density ? x * 2 : x;
            const int tile_y = density ? y * 2 : y;
            int water = session_->layer_value(CitySession::MapLayer::Water, tile_x, tile_y);
            if (density) {
                for (int dy = 0; dy < 2 && water == 0; ++dy) {
                    for (int dx = 0; dx < 2; ++dx) {
                        if (session_->layer_value(CitySession::MapLayer::Water, tile_x + dx, tile_y + dy) > 0) {
                            water = 1;
                            break;
                        }
                    }
                }
            }
            int value = 0;
            if (layer_ != CitySession::MapLayer::Water) {
                value = session_->layer_value(layer_, tile_x, tile_y);
            }
            if (density) {
                peak = std::max(peak, value);
            }
            next[static_cast<std::size_t>(y * cells_w + x)] = water > 0 ? -value - 1 : value;
        }
    }
    const unsigned serial = session_->map_serial();
    if (!cells_.empty() && cells_ == next && peak_ == peak && map_serial_ == serial && image_) {
        return;
    }
    cells_ = std::move(next);
    peak_ = peak;
    map_serial_ = serial;
    rebuild();
    map_.queue_draw();
}

void OverlayWindow::rebuild()
{
    const bool density = density_layer();
    const int cells_w = density ? CitySession::kWorldW / 2 : CitySession::kWorldW;
    const int cells_h = density ? CitySession::kWorldH / 2 : CitySession::kWorldH;
    if (!image_ || image_->get_width() != cells_w || image_->get_height() != cells_h) {
        image_ = Cairo::ImageSurface::create(Cairo::FORMAT_RGB24, cells_w, cells_h);
    }
    unsigned char *data = image_->get_data();
    const int stride = image_->get_stride();
    auto pack = [](const Rgb &color) {
        auto channel = [](double value) {
            const int byte = static_cast<int>(value * 255.0 + 0.5);
            return static_cast<std::uint32_t>(std::max(0, std::min(255, byte)));
        };
        return (channel(color.r) << 16) | (channel(color.g) << 8) | channel(color.b);
    };
    for (int y = 0; y < cells_h; ++y) {
        for (int x = 0; x < cells_w; ++x) {
            const int packed = cells_[static_cast<std::size_t>(y * cells_w + x)];
            const bool water = packed < 0;
            const int value = water ? -packed - 1 : packed;
            Rgb color{0.55, 0.42, 0.28};
            if (water && layer_ != CitySession::MapLayer::Water) {
                color = {0.10, 0.28, 0.72};
            }
            if (layer_ == CitySession::MapLayer::Power && !water) {
                switch (value) {
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
            } else if (layer_ == CitySession::MapLayer::Water) {
                color = water ? Rgb{0.15, 0.45, 0.95} : Rgb{0.45, 0.36, 0.24};
            } else if (!water && layer_ != CitySession::MapLayer::Power) {
                const double t = value / static_cast<double>(std::max(peak_, 1));
                if (t > 0) {
                    color = heat(layer_, t);
                }
            }
            auto *pixel = reinterpret_cast<std::uint32_t *>(data + y * stride + x * 4);
            *pixel = pack(color);
        }
    }
    image_->mark_dirty();
}

void OverlayWindow::click_at(double x, double y)
{
    GdkEventButton event{};
    event.button = 1;
    event.x = x;
    event.y = y;
    on_map_button(&event);
}

bool OverlayWindow::on_map_button(GdkEventButton *event)
{
    if (event == nullptr || event->button != 1) {
        return false;
    }
    double w = map_.get_allocated_width();
    double h = map_.get_allocated_height();
    // A click before the first allocation still maps onto the 4 px grid
    // the picture is drawn at.
    if (w < 2.0) {
        w = CitySession::kWorldW * 4.0;
    }
    if (h < 2.0) {
        h = CitySession::kWorldH * 4.0;
    }
    const double fx = std::max(0.0, std::min(1.0, event->x / w));
    const double fy = std::max(0.0, std::min(1.0, event->y / h));
    signal_jump.emit(fx, fy);
    return true;
}

bool OverlayWindow::on_draw_map(const Cairo::RefPtr<Cairo::Context> &cr)
{
    if (!image_) {
        sync();
    }
    if (!image_) {
        return true;
    }
    const double w = std::max(1, map_.get_allocated_width());
    const double h = std::max(1, map_.get_allocated_height());
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->save();
    cr->scale(w / image_->get_width(), h / image_->get_height());
    cr->set_source(image_, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr->cobj()), CAIRO_FILTER_NEAREST);
    cr->paint();
    cr->restore();
    return true;
}

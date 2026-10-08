// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// The Graphs legend must name every series beside its swatch.

#include "city_session.hpp"
#include "graphs_window.hpp"

#include <gdkmm/pixbuf.h>
#include <gtkmm/container.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

void pump()
{
    for (int i = 0; i < 8; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }
}

void walk(Gtk::Widget *widget, std::vector<Gtk::Widget *> &out)
{
    if (widget == nullptr) {
        return;
    }
    out.push_back(widget);
    if (auto *container = dynamic_cast<Gtk::Container *>(widget)) {
        for (Gtk::Widget *child : container->get_children()) {
            walk(child, out);
        }
    }
}

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

bool rect_of(Gtk::Widget &widget, Gtk::Widget &origin, Rect &out)
{
    int x = 0;
    int y = 0;
    if (!widget.translate_coordinates(origin, 0, 0, x, y)) {
        return false;
    }
    out.x = x;
    out.y = y;
    out.w = widget.get_allocated_width();
    out.h = widget.get_allocated_height();
    return out.w > 0 && out.h > 0;
}

bool intersects(const Rect &a, const Rect &b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

bool contains(const Rect &outer, const Rect &inner)
{
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

bool vertically_overlaps(const Rect &a, const Rect &b)
{
    return a.y < b.y + b.h && a.y + a.h > b.y;
}

// Text leaves pixels that are not the legend's background. A solid
// swatch or an empty label does not look like a line of glyphs.
bool text_ink_in(const Glib::RefPtr<Gdk::Pixbuf> &pix, const Rect &rect)
{
    if (!pix || rect.w < 2 || rect.h < 2) {
        return false;
    }
    const int channels = pix->get_n_channels();
    const int stride = pix->get_rowstride();
    const guint8 *pixels = pix->get_pixels();
    if (channels < 3 || pixels == nullptr) {
        return false;
    }
    const int x0 = std::max(0, rect.x);
    const int y0 = std::max(0, rect.y);
    const int x1 = std::min(pix->get_width(), rect.x + rect.w);
    const int y1 = std::min(pix->get_height(), rect.y + rect.h);
    if (x1 - x0 < 2 || y1 - y0 < 2) {
        return false;
    }

    // The paper color is the most common one. Antialiased glyphs sit on it
    // and must not be mistaken for a second background.
    int hist[16][16][16] = {};
    int best = 0;
    int paper_r = 0;
    int paper_g = 0;
    int paper_b = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const guint8 *p = pixels + y * stride + x * channels;
            const int bin_r = p[0] >> 4;
            const int bin_g = p[1] >> 4;
            const int bin_b = p[2] >> 4;
            const int n = ++hist[bin_r][bin_g][bin_b];
            if (n > best) {
                best = n;
                paper_r = (bin_r << 4) + 8;
                paper_g = (bin_g << 4) + 8;
                paper_b = (bin_b << 4) + 8;
            }
        }
    }

    int ink = 0;
    int seen = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const guint8 *p = pixels + y * stride + x * channels;
            const int delta = std::abs(p[0] - paper_r) + std::abs(p[1] - paper_g) + std::abs(p[2] - paper_b);
            ++seen;
            if (delta > 90) {
                ++ink;
            }
        }
    }
    // A readable word covers more than a speck and less than the whole box.
    return ink >= 12 && ink * 5 < seen * 4;
}

Gtk::Widget *find_named(const std::vector<Gtk::Widget *> &widgets, const char *name)
{
    for (Gtk::Widget *widget : widgets) {
        if (widget->get_name() == name) {
            return widget;
        }
    }
    return nullptr;
}

} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip or start its own server\n";
        return 1;
    }

    Gtk::Main kit(argc, argv);

    CitySession session;
    session.new_city("Legend", 42);
    GraphsWindow graphs;
    graphs.set_session(&session);
    graphs.present_graphs();
    pump();

    std::vector<Gtk::Widget *> widgets;
    walk(graphs.get_child(), widgets);

    Gtk::Widget *legend = find_named(widgets, "graph-legend");
    Gtk::Widget *chart = find_named(widgets, "graph-chart");
    if (legend == nullptr || chart == nullptr || !legend->get_visible() || !legend->get_mapped() ||
        !chart->get_visible() || !chart->get_mapped()) {
        return fail(1, "the graphs legend or chart is not on screen");
    }

    Rect legend_rect;
    Rect chart_rect;
    Rect window_rect{0, 0, graphs.get_allocated_width(), graphs.get_allocated_height()};
    if (!rect_of(*legend, graphs, legend_rect) || !rect_of(*chart, graphs, chart_rect) ||
        window_rect.w < 20 || window_rect.h < 20) {
        return fail(2, "the graphs window has no legend bounds");
    }
    if (intersects(legend_rect, chart_rect)) {
        return fail(3, "the legend overlaps the plot");
    }
    if (!contains(window_rect, legend_rect)) {
        return fail(4, "the legend is clipped by the graphs window");
    }

    const char *names[GraphsWindow::kSeriesCount] = {
        "Population", "Residential", "Commercial", "Industrial", "Cash flow", "Crime", "Pollution",
    };

    auto gdk = graphs.get_window();
    if (!gdk) {
        return fail(5, "the graphs window has no drawable");
    }
    gdk->process_updates(true);
    Glib::RefPtr<Gdk::Pixbuf> pix;
    try {
        pix = Gdk::Pixbuf::create(gdk, 0, 0, window_rect.w, window_rect.h);
    } catch (const Glib::Error &error) {
        std::cerr << error.what() << "\n";
        return fail(5, "could not render the graphs window");
    }

    for (const char *name : names) {
        Gtk::Label *label = nullptr;
        for (Gtk::Widget *widget : widgets) {
            auto *candidate = dynamic_cast<Gtk::Label *>(widget);
            if (candidate == nullptr || candidate->get_name() != "graph-legend-label") {
                continue;
            }
            const std::string text = candidate->get_text();
            if (text.rfind(name, 0) == 0) {
                label = candidate;
                break;
            }
        }
        if (label == nullptr) {
            std::cerr << "missing series " << name << "\n";
            return fail(6, "a graph series has no legend label");
        }
        const std::string text = label->get_text();
        if (!label->get_visible() || !label->get_mapped() || text.empty() || text == name ||
            text.find(':') == std::string::npos) {
            std::cerr << "label '" << text << "'\n";
            return fail(7, "a graph series label is empty or hidden");
        }

        auto layout = label->get_layout();
        int text_w = 0;
        int text_h = 0;
        if (!layout) {
            return fail(8, "a graph series label has no text layout");
        }
        layout->get_pixel_size(text_w, text_h);
        if (text_w <= 0 || text_h <= 0 || text_w > label->get_allocated_width() ||
            text_h > label->get_allocated_height()) {
            std::cerr << name << " layout " << text_w << "x" << text_h << " allocation "
                      << label->get_allocated_width() << "x" << label->get_allocated_height() << "\n";
            return fail(9, "a graph series label is clipped");
        }

        Gtk::Widget *row = label->get_parent();
        Gtk::DrawingArea *swatch = nullptr;
        if (auto *container = dynamic_cast<Gtk::Container *>(row)) {
            for (Gtk::Widget *child : container->get_children()) {
                if (child->get_name() == "graph-legend-swatch") {
                    swatch = dynamic_cast<Gtk::DrawingArea *>(child);
                }
            }
        }
        if (swatch == nullptr || !swatch->get_visible() || !swatch->get_mapped()) {
            return fail(10, "a graph series label has no swatch beside it");
        }

        Rect label_rect;
        Rect swatch_rect;
        if (!rect_of(*label, graphs, label_rect) || !rect_of(*swatch, graphs, swatch_rect)) {
            return fail(11, "a graph series label has no allocation");
        }
        if (!contains(window_rect, label_rect) || !contains(legend_rect, label_rect)) {
            std::cerr << name << " label " << label_rect.x << "," << label_rect.y << " " << label_rect.w
                      << "x" << label_rect.h << " legend " << legend_rect.x << "," << legend_rect.y << " "
                      << legend_rect.w << "x" << legend_rect.h << "\n";
            return fail(12, "a graph series label is outside the legend");
        }
        if (intersects(label_rect, chart_rect) || intersects(swatch_rect, chart_rect)) {
            return fail(13, "a graph series label overlaps the plot");
        }
        if (!vertically_overlaps(label_rect, swatch_rect) || label_rect.x < swatch_rect.x + swatch_rect.w - 1 ||
            label_rect.x > swatch_rect.x + swatch_rect.w + 32) {
            std::cerr << name << " label x " << label_rect.x << " swatch " << swatch_rect.x << "+"
                      << swatch_rect.w << "\n";
            return fail(14, "a graph series name is not next to its swatch");
        }
        if (!text_ink_in(pix, label_rect)) {
            // A busy display can hand back the window before the labels are
            // painted. Draw again and read the same rectangle.
            bool painted = false;
            for (int attempt = 0; attempt < 8 && !painted; ++attempt) {
                graphs.present();
                if (graphs.get_window()) {
                    graphs.get_window()->raise();
                }
                label->queue_draw();
                graphs.queue_draw();
                pump();
                gdk->process_updates(true);
                if (auto display = graphs.get_display()) {
                    display->sync();
                }
                g_usleep(50 * 1000);
                pump();
                try {
                    pix = Gdk::Pixbuf::create(gdk, 0, 0, graphs.get_allocated_width(),
                                              graphs.get_allocated_height());
                } catch (const Glib::Error &) {
                    continue;
                }
                if (!rect_of(*label, graphs, label_rect)) {
                    continue;
                }
                painted = text_ink_in(pix, label_rect);
            }
            if (!painted) {
                std::cerr << name << " rect " << label_rect.x << "," << label_rect.y << " " << label_rect.w
                          << "x" << label_rect.h << " text '" << text << "'\n";
                return fail(15, "a graph series label does not draw text inside the legend");
            }
        }
    }

    graphs.hide();
    return 0;
}

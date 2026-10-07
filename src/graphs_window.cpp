// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "graphs_window.hpp"

#include "city_session.hpp"
#include "graph_legend.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include <gtkmm/button.h>
#include <gtkmm/separator.h>

namespace {

std::string grouped(long value)
{
    const bool neg = value < 0;
    unsigned long mag = static_cast<unsigned long>(neg ? -value : value);
    std::string digits = std::to_string(mag);
    std::string text;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) {
            text.push_back(',');
        }
        text.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
    }
    std::reverse(text.begin(), text.end());
    return (neg ? "-" : "") + text;
}

std::string money(long value)
{
    const bool neg = value < 0;
    return (neg ? "-$" : "$") + grouped(neg ? -value : value);
}

struct Rgb {
    double r;
    double g;
    double b;
};

long population_at(const CitySession &session, CitySession::HistoryScale scale, int index)
{
    const int residential = session.history_value(CitySession::HistorySeries::Residential, scale, index);
    const int commercial = session.history_value(CitySession::HistorySeries::Commercial, scale, index);
    const int industrial = session.history_value(CitySession::HistorySeries::Industrial, scale, index);
    // resHist stores resPop/8. City population is (resPop + (comPop + indPop) * 8) * 20.
    return static_cast<long>(residential + commercial + industrial) * kHistoryPeoplePerSample;
}

} // namespace

GraphsWindow::GraphsWindow()
    : ten_(scale_group_, "10 years"), long_term_(scale_group_, "120 years")
{
    set_title("Graphs");
    set_border_width(12);
    set_default_size(640, 480);

    signal_delete_event().connect([this](GdkEventAny *) {
        hide();
        return true;
    });

    ten_.set_active(true);
    scales_.pack_start(ten_, Gtk::PACK_SHRINK);
    scales_.pack_start(long_term_, Gtk::PACK_SHRINK);

    population_.set_halign(Gtk::ALIGN_START);
    funds_.set_halign(Gtk::ALIGN_START);
    scale_note_.set_halign(Gtk::ALIGN_START);
    scale_note_.set_line_wrap(true);
    scale_note_.set_max_width_chars(64);

    chart_.set_hexpand(true);
    chart_.set_vexpand(true);
    chart_.set_size_request(560, 300);

    auto *head = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 16));
    head->pack_start(population_, Gtk::PACK_SHRINK);
    head->pack_start(funds_, Gtk::PACK_SHRINK);

    auto *close = Gtk::manage(new Gtk::Button("_Close", true));
    close->set_halign(Gtk::ALIGN_END);
    close->signal_clicked().connect([this] { hide(); });

    root_.pack_start(scales_, Gtk::PACK_SHRINK);
    root_.pack_start(*head, Gtk::PACK_SHRINK);
    root_.pack_start(scale_note_, Gtk::PACK_SHRINK);
    root_.pack_start(chart_, Gtk::PACK_EXPAND_WIDGET);
    root_.pack_start(*Gtk::manage(new Gtk::Separator()), Gtk::PACK_SHRINK);
    root_.pack_start(*close, Gtk::PACK_SHRINK);
    add(root_);

    chart_.signal_draw().connect(sigc::mem_fun(*this, &GraphsWindow::on_draw));
    ten_.signal_toggled().connect([this] {
        if (ten_.get_active()) {
            sync();
        }
    });
    long_term_.signal_toggled().connect([this] {
        if (long_term_.get_active()) {
            sync();
        }
    });
}

void GraphsWindow::set_session(CitySession *session)
{
    session_ = session;
}

void GraphsWindow::present_graphs()
{
    // The chart reads history samples. It does not need a vote, and a vote
    // would advance the simulator's random stream.
    show_all();
    present();
    sync();
}

void GraphsWindow::sync()
{
    if (session_ == nullptr) {
        return;
    }
    const CitySession::Evaluation report = session_->evaluation();
    population_.set_text("Population: " + grouped(report.population));
    funds_.set_text("Funds: " + money(session_->funds()));
    if (long_term_.get_active()) {
        scale_note_.set_text("120 yearly samples from the engine history. Each line is scaled to its own range. "
                             "The right edge is the newest year.");
    } else {
        scale_note_.set_text("120 monthly samples from the engine history. Each line is scaled to its own range. "
                             "The right edge is the newest month.");
    }
    chart_.queue_draw();
}

bool GraphsWindow::on_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const int width = chart_.get_allocated_width();
    const int height = chart_.get_allocated_height();
    if (width < 20 || height < 20) {
        return true;
    }

    auto style = chart_.get_style_context();
    const Gdk::RGBA fg = style->get_color(style->get_state());
    const bool dark = fg.get_red() + fg.get_green() + fg.get_blue() > 1.6;

    const int left = 16;
    const int right = 16;
    const int top = 12;
    const int legend_h = 92;
    const int bottom = legend_h + 8;
    const double plot_w = std::max(1, width - left - right);
    const double plot_h = std::max(1, height - top - bottom);
    const double plot_bottom = top + plot_h;

    cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), 0.18);
    cr->set_line_width(1);
    for (int i = 0; i <= 4; ++i) {
        const double y = top + plot_h * (i / 4.0);
        cr->move_to(left, y);
        cr->line_to(left + plot_w, y);
    }
    cr->stroke();
    cr->move_to(left, top);
    cr->line_to(left, plot_bottom);
    cr->line_to(left + plot_w, plot_bottom);
    cr->stroke();

    if (session_ == nullptr) {
        return true;
    }

    const auto scale =
        long_term_.get_active() ? CitySession::HistoryScale::Long : CitySession::HistoryScale::Short;

    struct Series {
        const char *name;
        Rgb color;
        bool population;
        bool cash;
        GraphLegendKind legend;
        CitySession::HistorySeries history;
    };
    const Series series[] = {
        {"Population", dark ? Rgb{0.95, 0.95, 0.95} : Rgb{0.15, 0.15, 0.15}, true, false,
         GraphLegendKind::Population, CitySession::HistorySeries::Residential},
        {"Residential", {0.20, 0.62, 0.28}, false, false, GraphLegendKind::People,
         CitySession::HistorySeries::Residential},
        {"Commercial", {0.20, 0.38, 0.82}, false, false, GraphLegendKind::People,
         CitySession::HistorySeries::Commercial},
        {"Industrial", dark ? Rgb{0.95, 0.78, 0.25} : Rgb{0.72, 0.55, 0.08}, false, false,
         GraphLegendKind::People, CitySession::HistorySeries::Industrial},
        {"Cash flow", {0.10, 0.55, 0.48}, false, true, GraphLegendKind::CashFlow,
         CitySession::HistorySeries::CashFlow},
        {"Crime", {0.80, 0.22, 0.18}, false, false, GraphLegendKind::Level, CitySession::HistorySeries::Crime},
        {"Pollution", dark ? Rgb{0.78, 0.62, 0.28} : Rgb{0.45, 0.38, 0.12}, false, false,
         GraphLegendKind::Level, CitySession::HistorySeries::Pollution},
    };

    auto sample = [&](const Series &item, int index) -> double {
        if (item.population) {
            return static_cast<double>(population_at(*session_, scale, index));
        }
        return session_->history_value(item.history, scale, index);
    };

    const int n = CitySession::kHistoryPoints;
    for (const Series &item : series) {
        double min_v = sample(item, 0);
        double max_v = min_v;
        for (int i = 1; i < n; ++i) {
            const double value = sample(item, i);
            min_v = std::min(min_v, value);
            max_v = std::max(max_v, value);
        }
        const bool flat = max_v <= min_v;
        if (flat) {
            max_v = min_v + 1.0;
        }
        cr->set_source_rgb(item.color.r, item.color.g, item.color.b);
        cr->set_line_width(item.population ? 2.4 : 1.6);
        for (int i = 0; i < n; ++i) {
            const int index = (n - 1) - i;
            const double value = sample(item, index);
            const double x = left + (n == 1 ? 0 : (plot_w * i) / (n - 1));
            const double y = flat ? (top + plot_h / 2.0)
                                  : (plot_bottom - ((value - min_v) / (max_v - min_v)) * plot_h);
            if (i == 0) {
                cr->move_to(x, y);
            } else {
                cr->line_to(x, y);
            }
        }
        cr->stroke();
    }

    const int columns = 2;
    const double row_h = 16;
    int row = 0;
    for (const Series &item : series) {
        const int column = row % columns;
        const int line = row / columns;
        const double x = left + column * (plot_w / columns);
        const double y = plot_bottom + 14 + line * row_h;
        cr->set_source_rgb(item.color.r, item.color.g, item.color.b);
        cr->set_line_width(3);
        cr->move_to(x, y + 4);
        cr->line_to(x + 16, y + 4);
        cr->stroke();

        const long newest = static_cast<long>(std::lround(sample(item, 0)));
        const std::string caption = graph_legend_caption(item.name, item.legend, newest);
        cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), 1);
        auto layout = create_pango_layout(caption);
        cr->move_to(x + 22, y - 4);
        layout->show_in_cairo_context(cr);
        ++row;
    }
    return true;
}

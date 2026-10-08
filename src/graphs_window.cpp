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
#include <gtkmm/grid.h>
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

struct SeriesSpec {
    const char *name;
    Rgb color;
    bool population;
    bool cash;
    GraphLegendKind legend;
    CitySession::HistorySeries history;
};

long population_at(const CitySession &session, CitySession::HistoryScale scale, int index)
{
    const int residential = session.history_value(CitySession::HistorySeries::Residential, scale, index);
    const int commercial = session.history_value(CitySession::HistorySeries::Commercial, scale, index);
    const int industrial = session.history_value(CitySession::HistorySeries::Industrial, scale, index);
    // resHist stores resPop/8. City population is (resPop + (comPop + indPop) * 8) * 20.
    return static_cast<long>(residential + commercial + industrial) * kHistoryPeoplePerSample;
}

void fill_series(bool dark, SeriesSpec out[GraphsWindow::kSeriesCount])
{
    out[0] = SeriesSpec{"Population", dark ? Rgb{0.95, 0.95, 0.95} : Rgb{0.15, 0.15, 0.15}, true, false,
                        GraphLegendKind::Population, CitySession::HistorySeries::Residential};
    out[1] = SeriesSpec{"Residential", {0.20, 0.62, 0.28}, false, false, GraphLegendKind::People,
                        CitySession::HistorySeries::Residential};
    out[2] = SeriesSpec{"Commercial", {0.20, 0.38, 0.82}, false, false, GraphLegendKind::People,
                        CitySession::HistorySeries::Commercial};
    out[3] = SeriesSpec{"Industrial", dark ? Rgb{0.95, 0.78, 0.25} : Rgb{0.72, 0.55, 0.08}, false, false,
                        GraphLegendKind::People, CitySession::HistorySeries::Industrial};
    out[4] = SeriesSpec{"Cash flow", {0.10, 0.55, 0.48}, false, true, GraphLegendKind::CashFlow,
                        CitySession::HistorySeries::CashFlow};
    out[5] = SeriesSpec{"Crime", {0.80, 0.22, 0.18}, false, false, GraphLegendKind::Level,
                        CitySession::HistorySeries::Crime};
    out[6] = SeriesSpec{"Pollution", dark ? Rgb{0.78, 0.62, 0.28} : Rgb{0.45, 0.38, 0.12}, false, false,
                        GraphLegendKind::Level, CitySession::HistorySeries::Pollution};
}

double sample_series(const CitySession &session, const SeriesSpec &item, CitySession::HistoryScale scale,
                     int index)
{
    if (item.population) {
        return static_cast<double>(population_at(session, scale, index));
    }
    if (item.cash) {
        return static_cast<double>(session.cash_flow_history(scale, index));
    }
    return session.history_value(item.history, scale, index);
}

} // namespace

GraphsWindow::GraphsWindow()
    : ten_(scale_group_, "10 years"), long_term_(scale_group_, "120 years")
{
    set_title("Graphs");
    set_border_width(12);
    // Tall enough for the plot plus a two-column legend at the theme font.
    // The window still grows when a caption is wider than this.
    set_default_size(720, 540);

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
    scale_note_.set_max_width_chars(72);

    chart_.set_name("graph-chart");
    chart_.set_hexpand(true);
    chart_.set_vexpand(true);
    chart_.set_size_request(560, 240);

    auto *head = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 16));
    head->pack_start(population_, Gtk::PACK_SHRINK);
    head->pack_start(funds_, Gtk::PACK_SHRINK);

    auto *legend = Gtk::manage(new Gtk::Grid());
    legend->set_name("graph-legend");
    legend->set_column_spacing(28);
    legend->set_row_spacing(4);
    legend->set_halign(Gtk::ALIGN_START);
    legend->set_hexpand(false);

    // The legend does not toggle series. A click on the name is the same
    // as a click on the swatch: it selects nothing.
    SeriesSpec specs[kSeriesCount];
    fill_series(false, specs);
    for (int i = 0; i < kSeriesCount; ++i) {
        auto *swatch = Gtk::manage(new Gtk::DrawingArea());
        swatch->set_name("graph-legend-swatch");
        swatch->set_size_request(18, 14);
        swatch->set_valign(Gtk::ALIGN_CENTER);
        swatch->set_halign(Gtk::ALIGN_START);
        legend_swatch_[i] = swatch;
        swatch_r_[i] = specs[i].color.r;
        swatch_g_[i] = specs[i].color.g;
        swatch_b_[i] = specs[i].color.b;
        swatch->signal_draw().connect([this, i](const Cairo::RefPtr<Cairo::Context> &cr) {
            Gtk::DrawingArea *area = legend_swatch_[i];
            const int w = std::max(1, area->get_allocated_width());
            const int h = std::max(1, area->get_allocated_height());
            auto style = area->get_style_context();
            style->render_background(cr, 0, 0, w, h);
            cr->set_source_rgb(swatch_r_[i], swatch_g_[i], swatch_b_[i]);
            cr->set_line_width(3.0);
            const double y = h / 2.0;
            cr->move_to(1.0, y);
            cr->line_to(static_cast<double>(std::max(2, w - 1)), y);
            cr->stroke();
            return true;
        });

        auto *label = Gtk::manage(new Gtk::Label(specs[i].name));
        label->set_name("graph-legend-label");
        label->set_halign(Gtk::ALIGN_START);
        label->set_valign(Gtk::ALIGN_CENTER);
        label->set_ellipsize(Pango::ELLIPSIZE_NONE);
        label->set_line_wrap(false);
        legend_label_[i] = label;

        auto *row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 6));
        row->set_name("graph-legend-row");
        row->pack_start(*swatch, Gtk::PACK_SHRINK);
        row->pack_start(*label, Gtk::PACK_SHRINK);
        legend->attach(*row, i % 2, i / 2, 1, 1);
    }

    auto *close = Gtk::manage(new Gtk::Button("_Close", true));
    close->set_halign(Gtk::ALIGN_END);
    close->signal_clicked().connect([this] { hide(); });

    root_.pack_start(scales_, Gtk::PACK_SHRINK);
    root_.pack_start(*head, Gtk::PACK_SHRINK);
    root_.pack_start(scale_note_, Gtk::PACK_SHRINK);
    root_.pack_start(chart_, Gtk::PACK_EXPAND_WIDGET);
    root_.pack_start(*legend, Gtk::PACK_SHRINK);
    root_.pack_start(*Gtk::manage(new Gtk::Separator()), Gtk::PACK_SHRINK);
    root_.pack_start(*close, Gtk::PACK_SHRINK);
    add(root_);

    chart_.signal_draw().connect(sigc::mem_fun(*this, &GraphsWindow::on_chart_draw));
    chart_.signal_style_updated().connect([this] { refresh_legend(); });
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
    refresh_legend();
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
        refresh_legend();
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
    refresh_legend();
    chart_.queue_draw();
}

bool GraphsWindow::chart_is_dark() const
{
    auto style = chart_.get_style_context();
    const Gdk::RGBA fg = style->get_color(style->get_state());
    return fg.get_red() + fg.get_green() + fg.get_blue() > 1.6;
}

void GraphsWindow::refresh_legend()
{
    SeriesSpec specs[kSeriesCount];
    fill_series(chart_is_dark(), specs);
    const auto scale =
        long_term_.get_active() ? CitySession::HistoryScale::Long : CitySession::HistoryScale::Short;
    for (int i = 0; i < kSeriesCount; ++i) {
        swatch_r_[i] = specs[i].color.r;
        swatch_g_[i] = specs[i].color.g;
        swatch_b_[i] = specs[i].color.b;
        if (legend_swatch_[i] != nullptr) {
            legend_swatch_[i]->queue_draw();
        }
        if (legend_label_[i] == nullptr) {
            continue;
        }
        if (session_ == nullptr) {
            legend_label_[i]->set_text(specs[i].name);
            continue;
        }
        const long newest = static_cast<long>(std::lround(sample_series(*session_, specs[i], scale, 0)));
        const bool exact = !specs[i].cash || session_->cash_flow_history_exact(scale, 0);
        legend_label_[i]->set_text(graph_legend_caption(specs[i].name, specs[i].legend, newest, exact));
    }
}

bool GraphsWindow::on_chart_draw(const Cairo::RefPtr<Cairo::Context> &cr)
{
    const int width = chart_.get_allocated_width();
    const int height = chart_.get_allocated_height();
    if (width < 20 || height < 20) {
        return true;
    }

    auto style = chart_.get_style_context();
    style->render_background(cr, 0, 0, width, height);
    const Gdk::RGBA fg = style->get_color(style->get_state());
    const bool dark = fg.get_red() + fg.get_green() + fg.get_blue() > 1.6;

    const int left = 16;
    const int right = 16;
    const int top = 12;
    const int bottom = 12;
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

    SeriesSpec series[kSeriesCount];
    fill_series(dark, series);

    const int n = CitySession::kHistoryPoints;
    for (const SeriesSpec &item : series) {
        double min_v = sample_series(*session_, item, scale, 0);
        double max_v = min_v;
        for (int i = 1; i < n; ++i) {
            const double value = sample_series(*session_, item, scale, i);
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
            const double value = sample_series(*session_, item, scale, index);
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
    return true;
}

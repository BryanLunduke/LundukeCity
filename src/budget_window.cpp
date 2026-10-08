// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "budget_window.hpp"

#include "city_session.hpp"

#include <algorithm>
#include <string>

#include <gtkmm/grid.h>
#include <gtkmm/separator.h>

namespace {

std::string money(long value)
{
    const bool neg = value < 0;
    unsigned long mag = static_cast<unsigned long>(neg ? -value : value);
    std::string digits = std::to_string(mag);
    std::string grouped;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
    }
    std::reverse(grouped.begin(), grouped.end());
    return (neg ? "-$" : "$") + grouped;
}

Gtk::Label *caption(const char *text)
{
    auto *label = Gtk::manage(new Gtk::Label(text));
    label->set_halign(Gtk::ALIGN_START);
    return label;
}

} // namespace

BudgetWindow::BudgetWindow()
    : tax_(Gtk::ORIENTATION_HORIZONTAL),
      road_(Gtk::ORIENTATION_HORIZONTAL),
      police_(Gtk::ORIENTATION_HORIZONTAL),
      fire_(Gtk::ORIENTATION_HORIZONTAL)
{
    set_title("Budget");
    set_border_width(12);
    set_default_size(560, 420);

    closing_note_.set_halign(Gtk::ALIGN_START);
    closing_note_.set_xalign(0);
    closing_note_.set_line_wrap(true);
    closing_note_.set_max_width_chars(52);
    closing_note_.set_no_show_all(true);
    closing_note_.hide();

    signal_delete_event().connect([this](GdkEventAny *) {
        // Title-bar close collects a waiting year at the rates on screen,
        // and applies a menu budget the same way OK does.
        accept_on_hide_ = true;
        hide();
        return true;
    });
    signal_hide().connect([this] {
        if (session_ == nullptr) {
            return;
        }
        if (!accept_on_hide_) {
            restore_open_rates();
        }
        session_->finish_budget_edit();
    });

    auto tune = [](Gtk::Scale &scale, double upper) {
        scale.set_range(0, upper);
        scale.set_increments(1, upper > 20 ? 10 : 5);
        scale.set_digits(0);
        scale.set_draw_value(false);
        scale.set_hexpand(true);
    };
    tune(tax_, 20);
    tune(road_, 100);
    tune(police_, 100);
    tune(fire_, 100);

    for (Gtk::Label *label : {&taxes_, &cash_flow_, &funds_, &projected_, &tax_value_}) {
        label->set_halign(Gtk::ALIGN_START);
        label->set_xalign(0);
    }
    // Cut sentences are one long line. Wrap them so the book stays near
    // its 560 px default instead of growing out to the full sentence.
    for (Gtk::Label *label : {&road_value_, &police_value_, &fire_value_}) {
        label->set_halign(Gtk::ALIGN_START);
        label->set_xalign(0);
        label->set_line_wrap(true);
        label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
        label->set_max_width_chars(52);
        label->set_width_chars(48);
    }

    auto *grid = Gtk::manage(new Gtk::Grid());
    grid->set_column_spacing(16);
    grid->set_row_spacing(4);
    grid->attach(taxes_, 0, 0, 1, 1);
    grid->attach(funds_, 1, 0, 1, 1);
    grid->attach(cash_flow_, 0, 1, 1, 1);
    grid->attach(projected_, 1, 1, 1, 1);

    auto add_slider = [&](const char *title, Gtk::Label &value, Gtk::Scale &scale) -> Gtk::Label * {
        auto *name = caption(title);
        name->set_margin_top(8);
        root_.pack_start(*name, Gtk::PACK_SHRINK);
        root_.pack_start(value, Gtk::PACK_SHRINK);
        root_.pack_start(scale, Gtk::PACK_SHRINK);
        return name;
    };

    root_.pack_start(*grid, Gtk::PACK_SHRINK);
    root_.pack_start(closing_note_, Gtk::PACK_SHRINK);
    root_.pack_start(*Gtk::manage(new Gtk::Separator()), Gtk::PACK_SHRINK);
    tax_title_ = add_slider("Tax rate", tax_value_, tax_);
    add_slider("Road fund", road_value_, road_);
    add_slider("Police fund", police_value_, police_);
    add_slider("Fire fund", fire_value_, fire_);

    auto *buttons = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
    buttons->set_halign(Gtk::ALIGN_END);
    dismiss_.signal_clicked().connect([this] {
        if (session_ != nullptr && session_->budget_pending()) {
            // Reset puts the opening rates back and leaves the year unpaid.
            restore_open_rates();
            sync();
            return;
        }
        accept_on_hide_ = false;
        hide();
    });
    accept_.signal_clicked().connect([this] {
        accept_on_hide_ = true;
        hide();
    });
    buttons->pack_start(dismiss_, Gtk::PACK_SHRINK);
    buttons->pack_start(accept_, Gtk::PACK_SHRINK);
    root_.pack_start(*buttons, Gtk::PACK_SHRINK);
    apply_mode();

    add(root_);

    // A scale can report a fraction that rounds to the percent already
    // showing, including 0. That is not a new request. Snapping the
    // scale keeps the cut sentence. An explicit move to a different
    // percent, including a real move to 0%, still goes through.
    auto on_scale = [this](Gtk::Scale &scale, int shown, auto apply) {
        if (updating_ || session_ == nullptr) {
            return;
        }
        const int percent = static_cast<int>(scale.get_value() + 0.5);
        if (percent == shown) {
            if (scale.get_value() != static_cast<double>(percent)) {
                updating_ = true;
                scale.set_value(percent);
                updating_ = false;
            }
            return;
        }
        apply(percent);
        sync();
    };
    tax_.signal_value_changed().connect([this, on_scale] {
        on_scale(tax_, session_ == nullptr ? -1 : session_->tax(), [this](int percent) { session_->set_tax(percent); });
    });
    road_.signal_value_changed().connect([this, on_scale] {
        on_scale(road_, session_ == nullptr ? -1 : session_->budget().road_percent,
                 [this](int percent) { session_->set_road_funding(percent); });
    });
    police_.signal_value_changed().connect([this, on_scale] {
        on_scale(police_, session_ == nullptr ? -1 : session_->budget().police_percent,
                 [this](int percent) { session_->set_police_funding(percent); });
    });
    fire_.signal_value_changed().connect([this, on_scale] {
        on_scale(fire_, session_ == nullptr ? -1 : session_->budget().fire_percent,
                 [this](int percent) { session_->set_fire_funding(percent); });
    });
}

void BudgetWindow::set_session(CitySession *session)
{
    session_ = session;
}

void BudgetWindow::present_book()
{
    if (session_ != nullptr) {
        const CitySession::BudgetBook book = session_->budget();
        open_tax_ = book.tax_percent;
        open_road_ = book.road_percent;
        open_police_ = book.police_percent;
        open_fire_ = book.fire_percent;
    }
    accept_on_hide_ = true;
    apply_mode();
    show_all();
    present();
    sync();
}

void BudgetWindow::restore_open_rates()
{
    if (session_ == nullptr) {
        return;
    }
    session_->restore_budget_rates(open_tax_, open_road_, open_police_, open_fire_);
}

void BudgetWindow::apply_mode()
{
    const bool tax_year = session_ != nullptr && session_->budget_pending();
    if (tax_year) {
        dismiss_.set_label("_Reset");
        accept_.set_label("_Collect Taxes");
        closing_note_.set_text("This year is collected when the window closes.");
        closing_note_.show();
    } else {
        dismiss_.set_label("_Cancel");
        accept_.set_label("_OK");
        closing_note_.hide();
    }
    dismiss_.set_use_underline(true);
    accept_.set_use_underline(true);
    if (tax_title_ != nullptr) {
        tax_title_->set_text(tax_year ? "Tax rate" : "Next year's tax rate");
    }
}

void BudgetWindow::set_funding_quietly(Gtk::Scale &scale, int percent)
{
    if (static_cast<int>(scale.get_value() + 0.5) != percent) {
        scale.set_value(percent);
    }
}

void BudgetWindow::sync()
{
    if (session_ == nullptr) {
        return;
    }
    const CitySession::BudgetBook book = session_->budget();
    apply_mode();
    if (session_->budget_pending()) {
        taxes_.set_text("Taxes collected: " + money(book.taxes));
    } else if (book.estimates || !book.taxes_known) {
        taxes_.set_text("Estimated taxes, not yet collected: " + money(book.taxes));
    } else {
        taxes_.set_text("Last January's taxes: " + money(book.taxes));
    }
    if (!session_->budget_pending() && book.estimates) {
        closing_note_.set_text("The first tax year has not been collected yet. These figures are estimates.");
        closing_note_.show();
    }
    cash_flow_.set_text(std::string(book.estimates ? "Estimated cash flow: " : "Cash flow: ") +
                        (book.cash_flow > 0 ? "+" : "") + money(book.cash_flow));
    funds_.set_text("Previous funds: " + money(book.previous_funds));
    projected_.set_text("Current funds: " + money(book.funds));

    updating_ = true;
    set_funding_quietly(tax_, book.tax_percent);
    set_funding_quietly(road_, book.road_percent);
    set_funding_quietly(police_, book.police_percent);
    set_funding_quietly(fire_, book.fire_percent);
    updating_ = false;

    const char *request = book.estimates ? "Estimated request " : "Request ";
    auto funding_line = [request](long need, int percent, long spent, const std::string &note) {
        if (note.empty()) {
            return std::string(request) + money(need) + "    " + std::to_string(percent) + "% = " + money(spent);
        }
        return std::string(request) + money(need) + "    " + note + " = " + money(spent);
    };
    tax_value_.set_text(std::to_string(book.tax_percent) + "%");
    road_value_.set_text(funding_line(book.road_need, book.road_percent, book.road_spent, book.road_note));
    police_value_.set_text(
        funding_line(book.police_need, book.police_percent, book.police_spent, book.police_note));
    fire_value_.set_text(funding_line(book.fire_need, book.fire_percent, book.fire_spent, book.fire_note));
}

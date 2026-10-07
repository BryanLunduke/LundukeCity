// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "evaluation_window.hpp"

#include "city_session.hpp"

#include <algorithm>
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

Gtk::Label *caption(const char *text)
{
    auto *label = Gtk::manage(new Gtk::Label());
    label->set_markup(std::string("<b>") + text + "</b>");
    label->set_halign(Gtk::ALIGN_START);
    label->set_margin_top(6);
    return label;
}

} // namespace

EvaluationWindow::EvaluationWindow()
{
    set_title("Evaluation");
    set_border_width(12);
    set_default_size(460, 520);

    signal_delete_event().connect([this](GdkEventAny *) {
        hide();
        return true;
    });

    for (Gtk::Label *label : {&heading_, &score_, &score_delta_, &yes_, &no_, &problems_empty_, &population_,
                              &migration_, &assessed_, &category_, &difficulty_}) {
        label->set_halign(Gtk::ALIGN_START);
        label->set_xalign(0);
    }
    for (Gtk::Label &problem : problem_) {
        problem.set_halign(Gtk::ALIGN_START);
        problem.set_xalign(0);
    }
    heading_.set_line_wrap(true);
    opinion_.set_show_text(true);
    opinion_.set_fraction(0.5);

    root_.pack_start(heading_, Gtk::PACK_SHRINK);
    root_.pack_start(*caption("Score"), Gtk::PACK_SHRINK);
    root_.pack_start(score_, Gtk::PACK_SHRINK);
    root_.pack_start(score_delta_, Gtk::PACK_SHRINK);

    root_.pack_start(*caption("Public opinion"), Gtk::PACK_SHRINK);
    auto *ask = Gtk::manage(new Gtk::Label("Is the mayor doing a good job?"));
    ask->set_halign(Gtk::ALIGN_START);
    root_.pack_start(*ask, Gtk::PACK_SHRINK);
    auto *votes = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 16));
    votes->pack_start(yes_, Gtk::PACK_SHRINK);
    votes->pack_start(no_, Gtk::PACK_SHRINK);
    root_.pack_start(*votes, Gtk::PACK_SHRINK);
    root_.pack_start(opinion_, Gtk::PACK_SHRINK);

    root_.pack_start(*caption("Worst problems"), Gtk::PACK_SHRINK);
    root_.pack_start(problems_empty_, Gtk::PACK_SHRINK);
    for (Gtk::Label &problem : problem_) {
        root_.pack_start(problem, Gtk::PACK_SHRINK);
    }

    root_.pack_start(*caption("Statistics"), Gtk::PACK_SHRINK);
    root_.pack_start(population_, Gtk::PACK_SHRINK);
    root_.pack_start(migration_, Gtk::PACK_SHRINK);
    root_.pack_start(assessed_, Gtk::PACK_SHRINK);
    root_.pack_start(category_, Gtk::PACK_SHRINK);
    root_.pack_start(difficulty_, Gtk::PACK_SHRINK);

    auto *close = Gtk::manage(new Gtk::Button("_Close", true));
    close->set_halign(Gtk::ALIGN_END);
    close->set_margin_top(8);
    close->signal_clicked().connect([this] { hide(); });
    root_.pack_start(*Gtk::manage(new Gtk::Separator()), Gtk::PACK_SHRINK);
    root_.pack_start(*close, Gtk::PACK_SHRINK);
    add(root_);
}

void EvaluationWindow::set_session(CitySession *session)
{
    session_ = session;
}

void EvaluationWindow::present_report()
{
    if (session_ != nullptr) {
        session_->update_evaluation();
        month_index_ = session_->game_month_index();
    }
    show_all();
    present();
    sync();
}

void EvaluationWindow::note_month(int month_index)
{
    if (session_ == nullptr || month_index == month_index_) {
        return;
    }
    month_index_ = month_index;
    session_->update_evaluation();
}

void EvaluationWindow::sync()
{
    if (session_ == nullptr) {
        return;
    }
    const CitySession::Evaluation report = session_->evaluation();
    heading_.set_text(session_->city_name() + " — " + session_->date_text());
    score_.set_text("Score: " + std::to_string(report.score));
    if (report.score_delta > 0) {
        score_delta_.set_text("Change since the last evaluation: +" + std::to_string(report.score_delta));
    } else if (report.score_delta < 0) {
        score_delta_.set_text("Change since the last evaluation: " + std::to_string(report.score_delta));
    } else {
        score_delta_.set_text("Change since the last evaluation: none");
    }

    const int no_percent = 100 - report.yes_percent;
    yes_.set_text("Yes: " + std::to_string(report.yes_percent) + "%");
    no_.set_text("No: " + std::to_string(no_percent) + "%");
    opinion_.set_fraction(std::max(0.0, std::min(1.0, report.yes_percent / 100.0)));
    opinion_.set_text("Yes " + std::to_string(report.yes_percent) + "%");

    const bool any = !report.problems.empty();
    problems_empty_.set_visible(!any);
    problems_empty_.set_text(any ? "" : "No complaints recorded.");
    for (int i = 0; i < 4; ++i) {
        if (i < static_cast<int>(report.problems.size())) {
            const CitySession::Problem &problem = report.problems[static_cast<std::size_t>(i)];
            problem_[i].set_text(problem.name + ": " + std::to_string(problem.votes) + "%");
            problem_[i].set_visible(true);
        } else {
            problem_[i].set_text("");
            problem_[i].set_visible(false);
        }
    }

    population_.set_text("Population: " + grouped(report.population));
    const std::string migration = grouped(report.migration);
    migration_.set_text("Net migration (last year): " + migration);
    assessed_.set_text("Assessed value: " + money(report.assessed_value));
    category_.set_text("Category: " + report.category);
    difficulty_.set_text("Difficulty: " + report.difficulty);
}

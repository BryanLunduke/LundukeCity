// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/window.h>

class CitySession;

// Score, public opinion, and the problem list from the engine evaluation.
class EvaluationWindow : public Gtk::Window {
public:
    EvaluationWindow();

    void set_session(CitySession *session);
    void present_report();
    // Runs the preview when the game month changes. sync() only copies labels.
    void note_month(int month_index);
    void sync();

private:
    CitySession *session_ = nullptr;
    int month_index_ = -1;

    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 8};
    Gtk::Label heading_;
    Gtk::Label score_;
    Gtk::Label score_delta_;
    Gtk::Label yes_;
    Gtk::Label no_;
    Gtk::ProgressBar opinion_;
    Gtk::Label problems_empty_;
    Gtk::Label problem_[4];
    Gtk::Label population_;
    Gtk::Label migration_;
    Gtk::Label assessed_;
    Gtk::Label category_;
    Gtk::Label difficulty_;
};

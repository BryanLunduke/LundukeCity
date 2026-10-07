// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/window.h>

class CitySession;

// Funding book: tax rate plus road, police, and fire funding, with the
// income and expenses the engine is currently using.
class BudgetWindow : public Gtk::Window {
public:
    BudgetWindow();

    void set_session(CitySession *session);
    void present_book();
    void sync();

private:
    void set_funding_quietly(Gtk::Scale &scale, int percent);

    CitySession *session_ = nullptr;
    bool updating_ = false;
    bool accept_on_hide_ = true;
    int open_tax_ = 7;
    int open_road_ = 100;
    int open_police_ = 100;
    int open_fire_ = 100;

    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 8};
    Gtk::Label taxes_;
    Gtk::Label cash_flow_;
    Gtk::Label funds_;
    Gtk::Label projected_;

    Gtk::Label tax_value_;
    Gtk::Scale tax_;
    Gtk::Label road_value_;
    Gtk::Scale road_;
    Gtk::Label police_value_;
    Gtk::Scale police_;
    Gtk::Label fire_value_;
    Gtk::Scale fire_;
};

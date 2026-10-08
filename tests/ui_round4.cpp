// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Window regressions for the round-4 review: message bar (finding 6),
// announcement pause (finding 2), and the budget buttons (findings 4 and 7).

#include "app_window.hpp"
#include "budget_window.hpp"
#include "city_session.hpp"

#include <gtkmm/button.h>
#include <gtkmm/container.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/scale.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

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
    for (int i = 0; i < 4; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }
}

void walk_widgets(Gtk::Widget *widget, std::vector<Gtk::Widget *> &out)
{
    if (widget == nullptr) {
        return;
    }
    out.push_back(widget);
    if (auto *container = dynamic_cast<Gtk::Container *>(widget)) {
        for (Gtk::Widget *child : container->get_children()) {
            walk_widgets(child, out);
        }
    }
}

Gtk::Button *find_button(const std::vector<Gtk::Widget *> &widgets, const char *label)
{
    for (Gtk::Widget *widget : widgets) {
        if (auto *button = dynamic_cast<Gtk::Button *>(widget)) {
            if (button->get_label() == label && button->get_use_underline()) {
                return button;
            }
        }
    }
    return nullptr;
}

bool label_contains(const std::vector<Gtk::Widget *> &widgets, const std::string &text)
{
    for (Gtk::Widget *widget : widgets) {
        if (auto *label = dynamic_cast<Gtk::Label *>(widget)) {
            if (label->get_visible() && label->get_text().find(text) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

std::vector<Gtk::Scale *> scales_of(const std::vector<Gtk::Widget *> &widgets)
{
    std::vector<Gtk::Scale *> scales;
    for (Gtk::Widget *widget : widgets) {
        if (auto *scale = dynamic_cast<Gtk::Scale *>(widget)) {
            scales.push_back(scale);
        }
    }
    return scales;
}

bool close_from_titlebar(Gtk::Window &window)
{
    pump();
    const auto gdk = window.get_window();
    if (!gdk) {
        return false;
    }
    GdkEvent event{};
    event.any.type = GDK_DELETE;
    event.any.window = gdk->gobj();
    event.any.send_event = 1;
    gtk_widget_event(GTK_WIDGET(window.gobj()), &event);
    pump();
    return true;
}

int budget_window_cases()
{
    CitySession session;
    session.new_city("Budget", 40);
    if (hostile_review_session_probe(session, 14) != 100) {
        return fail(10, "could not open a tax year for the budget window");
    }
    const long treasury = session.funds();
    BudgetWindow book;
    book.set_session(&session);
    book.present_book();
    pump();
    const int open_road = session.budget().road_percent;

    std::vector<Gtk::Widget *> widgets;
    walk_widgets(book.get_child(), widgets);
    Gtk::Button *reset = find_button(widgets, "_Reset");
    Gtk::Button *collect = find_button(widgets, "_Collect Taxes");
    if (reset == nullptr || collect == nullptr || find_button(widgets, "_Cancel") != nullptr ||
        find_button(widgets, "_OK") != nullptr) {
        return fail(11, "a tax year offered Cancel or OK instead of Reset and Collect Taxes");
    }
    if (!label_contains(widgets, "This year is collected when the window closes.") ||
        !label_contains(widgets, "Taxes collected:") || !label_contains(widgets, "Tax rate") ||
        label_contains(widgets, "Next year's tax rate") || label_contains(widgets, "Last January")) {
        return fail(12, "the tax-year budget did not say the year is collected when the window closes");
    }
    if (!label_contains(widgets, "cut to")) {
        return fail(13, "the tax-year budget did not say the funding was cut");
    }

    const auto scales = scales_of(widgets);
    if (scales.size() < 2) {
        return fail(14, "the budget window is missing its sliders");
    }
    scales[1]->set_value(0);
    pump();
    if (session.budget().road_percent != 0 || session.funds() != treasury) {
        return fail(15, "the road slider did not move, or it collected the year");
    }
    reset->clicked();
    pump();
    if (!book.get_visible() || !session.budget_pending() || session.funds() != treasury ||
        session.budget().road_percent != open_road) {
        std::cerr << "visible " << book.get_visible() << " pending " << session.budget_pending() << " funds "
                  << session.funds() << " road " << session.budget().road_percent << " open " << open_road << "\n";
        return fail(16, "Reset collected the year, closed the window, or left the edited rate");
    }

    scales[1]->set_value(0);
    pump();
    const long projected = session.budget().funds;
    collect->clicked();
    pump();
    if (book.get_visible() || session.budget_pending() || session.funds() != projected) {
        std::cerr << "visible " << book.get_visible() << " pending " << session.budget_pending() << " funds "
                  << session.funds() << " projected " << projected << "\n";
        return fail(17, "Collect Taxes did not post the year at the shown rates");
    }

    if (hostile_review_session_probe(session, 14) != 100) {
        return fail(18, "could not open a second tax year for the title-bar close");
    }
    book.present_book();
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    const auto title_scales = scales_of(widgets);
    if (title_scales.size() < 2) {
        return fail(19, "the second tax year lost its sliders");
    }
    title_scales[1]->set_value(0);
    pump();
    const long title_projected = session.budget().funds;
    if (!close_from_titlebar(book) || book.get_visible() || session.budget_pending() ||
        session.funds() != title_projected) {
        std::cerr << "visible " << book.get_visible() << " pending " << session.budget_pending() << " funds "
                  << session.funds() << " projected " << title_projected << "\n";
        return fail(20, "the title-bar close did not collect the year at the shown rates");
    }

    session.set_tax(7);
    session.set_road_funding(100);
    if (hostile_review_session_probe(session, 15) != 2000 || session.budget_pending()) {
        return fail(21, "could not stage a menu budget with last January's taxes");
    }
    book.present_book();
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    Gtk::Button *cancel = find_button(widgets, "_Cancel");
    Gtk::Button *ok = find_button(widgets, "_OK");
    if (cancel == nullptr || ok == nullptr || find_button(widgets, "_Reset") != nullptr ||
        find_button(widgets, "_Collect Taxes") != nullptr) {
        return fail(22, "the menu budget did not offer Cancel and OK");
    }
    if (!label_contains(widgets, "Next year's tax rate") ||
        !label_contains(widgets, "Last January's taxes: $2,000") ||
        label_contains(widgets, "This year is collected when the window closes.") ||
        label_contains(widgets, "Taxes collected:")) {
        return fail(23, "the menu budget still reads as if the tax slider collects this year");
    }
    const auto menu_scales = scales_of(widgets);
    if (menu_scales.size() < 2) {
        return fail(24, "the menu budget lost its sliders");
    }
    menu_scales[0]->set_value(20);
    menu_scales[1]->set_value(25);
    pump();
    if (session.tax() != 20 || session.budget().taxes != 2000 ||
        !label_contains(widgets, "Last January's taxes: $2,000")) {
        std::cerr << "tax " << session.tax() << " collected " << session.budget().taxes << "\n";
        return fail(25, "dragging the menu tax slider changed last January's receipt");
    }
    cancel->clicked();
    pump();
    if (book.get_visible() || session.tax() != 7 || session.budget().road_percent != 100) {
        std::cerr << "tax " << session.tax() << " road " << session.budget().road_percent << "\n";
        return fail(26, "Cancel on a menu budget did not restore the rates from when it opened");
    }

    const int full_effect = hostile_review_session_probe(session, 3);
    book.present_book();
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    ok = find_button(widgets, "_OK");
    const auto ok_scales = scales_of(widgets);
    if (ok == nullptr || ok_scales.size() < 2 || full_effect <= 0) {
        return fail(27, "could not stage full road maintenance for OK");
    }
    ok_scales[1]->set_value(0);
    pump();
    if (hostile_review_session_probe(session, 4) != full_effect) {
        return fail(28, "the road slider applied maintenance before OK");
    }
    ok->clicked();
    pump();
    if (book.get_visible() || session.budget().road_percent != 0 ||
        hostile_review_session_probe(session, 4) != 0) {
        std::cerr << "road " << session.budget().road_percent << " effect " << hostile_review_session_probe(session, 4)
                  << "\n";
        return fail(29, "OK on a menu budget did not apply the rates on screen");
    }
    return 0;
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
    AppWindow window;
    window.show_all();
    pump();

    int code = 0;
    if (hostile_review_window_probe(window, 1) != 1) {
        code = fail(1, "Loaded a saved city. did not reach the message bar");
    } else if (hostile_review_window_probe(window, 2) != 1) {
        code = fail(2, "Playing a scenario did not reach the message bar");
    } else if (hostile_review_window_probe(window, 3) != 1) {
        code = fail(3, "a win did not remember Fast for the pause key and Keep playing");
    } else {
        code = budget_window_cases();
    }

    window.hide();
    return code;
}

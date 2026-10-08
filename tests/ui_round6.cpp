// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Window regressions for the round-6 review.

#include "app_window.hpp"
#include "budget_window.hpp"
#include "city_session.hpp"

#include <glibmm/main.h>
#include <gtkmm/button.h>
#include <gtkmm/container.h>
#include <gtkmm/dialog.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/scale.h>
#include <gtkmm/treeview.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
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

std::string label_text_containing(const std::vector<Gtk::Widget *> &widgets, const std::string &text)
{
    for (Gtk::Widget *widget : widgets) {
        if (auto *label = dynamic_cast<Gtk::Label *>(widget)) {
            if (label->get_visible() && label->get_text().find(text) != std::string::npos) {
                return label->get_text();
            }
        }
    }
    return {};
}

std::string visible_label_blob(Gtk::Widget *widget)
{
    std::vector<Gtk::Widget *> widgets;
    walk_widgets(widget, widgets);
    std::string blob;
    for (Gtk::Widget *item : widgets) {
        auto *label = dynamic_cast<Gtk::Label *>(item);
        if (label != nullptr && label->get_visible()) {
            blob += label->get_text();
            blob.push_back('\n');
        }
    }
    return blob;
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

Gtk::TreeView *find_tree(Gtk::Widget *widget)
{
    if (widget == nullptr) {
        return nullptr;
    }
    if (auto *view = dynamic_cast<Gtk::TreeView *>(widget)) {
        return view;
    }
    if (auto *container = dynamic_cast<Gtk::Container *>(widget)) {
        for (Gtk::Widget *child : container->get_children()) {
            if (Gtk::TreeView *found = find_tree(child)) {
                return found;
            }
        }
    }
    return nullptr;
}

int budget_cases()
{
    CitySession fresh;
    fresh.new_city("Estimate", 30);
    BudgetWindow estimate;
    estimate.set_session(&fresh);
    estimate.present_book();
    pump();
    std::vector<Gtk::Widget *> widgets;
    walk_widgets(estimate.get_child(), widgets);
    if (label_contains(widgets, "in this file") || label_contains(widgets, "No tax receipt") ||
        !label_contains(widgets, "not yet collected") || !label_contains(widgets, "Estimated") ||
        !label_contains(widgets, "These figures are estimates.")) {
        return fail(30, "a new city still says there is no tax receipt or hides the estimate");
    }
    estimate.hide();

    CitySession tokyo;
    if (!tokyo.load_scenario(CitySession::scenario_def(4).id) ||
        std::string(CitySession::scenario_def(4).name) != "Tokyo") {
        return fail(31, "could not load Tokyo for the estimate");
    }
    const CitySession::BudgetBook tokyo_book = tokyo.budget();
    if (!tokyo_book.estimates || tokyo_book.road_need <= 0 || tokyo_book.taxes <= 0) {
        std::cerr << "tokyo road " << tokyo_book.road_need << " tax " << tokyo_book.taxes << "\n";
        return fail(32, "Tokyo's estimate is still a zero bill");
    }
    BudgetWindow tokyo_window;
    tokyo_window.set_session(&tokyo);
    tokyo_window.present_book();
    pump();
    widgets.clear();
    walk_widgets(tokyo_window.get_child(), widgets);
    const std::string road = label_text_containing(widgets, "Estimated request");
    if (road.empty() || road.find("Estimated request $0") != std::string::npos ||
        label_contains(widgets, "No tax receipt") || label_contains(widgets, "in this file")) {
        std::cerr << "road line '" << road << "'\n";
        return fail(33, "Tokyo's budget still shows a $0 department or a missing receipt");
    }
    tokyo_window.hide();

    CitySession taxed;
    taxed.new_city("Cut", 31);
    if (hostile_review_session_probe(taxed, 16) != 100) {
        return fail(34, "could not open the cut tax year");
    }
    BudgetWindow book;
    book.set_session(&taxed);
    book.present_book();
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    const auto scales = scales_of(widgets);
    if (scales.size() < 4) {
        return fail(35, "the budget window lost a slider");
    }
    const std::string fire_before = label_text_containing(widgets, "earlier department");
    if (fire_before.empty()) {
        return fail(36, "the fire row did not explain the cut to 0%");
    }
    scales[3]->set_value(0.4);
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    const std::string fire_after = label_text_containing(widgets, "earlier department");
    if (fire_after != fire_before || taxed.budget().fire_percent != 0) {
        std::cerr << "after '" << fire_after << "' pct " << taxed.budget().fire_percent << "\n";
        return fail(37, "a scale event that rounds to 0 cleared the cut sentence");
    }
    scales[1]->set_value(42.4);
    pump();
    widgets.clear();
    walk_widgets(book.get_child(), widgets);
    if (!label_contains(widgets, "cut to 42%")) {
        return fail(38, "a scale event that stays on 42% cleared the road sentence");
    }

    int min_w = 0;
    int nat_w = 0;
    book.get_child()->get_preferred_width(min_w, nat_w);
    int window_w = 0;
    int window_h = 0;
    book.get_size(window_w, window_h);
    if (min_w >= 760 || nat_w >= 760 || window_w >= 760 || nat_w < 200) {
        std::cerr << "min " << min_w << " natural " << nat_w << " window " << window_w << "x" << window_h << "\n";
        return fail(39, "the cut sentences widened the budget window");
    }
    book.hide();
    return 0;
}

bool file_exists(const std::string &path)
{
    std::ifstream in(path);
    return in.good();
}

} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip or start its own server\n";
        return 1;
    }

    char home_template[] = "/tmp/lunduke-round6-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return fail(1, "could not make a home directory for the welcome flag");
    }
    setenv("HOME", home, 1);
    setenv("XDG_CONFIG_HOME", "/tmp/xfce4-must-not-be-written", 1);
    setenv("XDG_CONFIG_DIRS", "/etc/xdg", 1);

    Gtk::Main kit(argc, argv);
    AppWindow window;
    window.show_all();
    pump();

    const int startup = hostile_review_window_probe(window, 6);
    if (startup != (1 | 2 | 4 | 8 | 16)) {
        std::cerr << "startup bits " << startup << "\n";
        return fail(2, "the first screen is running, the welcome is missing, or the tools have no tooltip");
    }

    bool saw_goals = false;
    sigc::connection list_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::Dialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Play Scenario") {
                continue;
            }
            Gtk::TreeView *view = find_tree(dialog);
            if (view == nullptr || !view->get_model()) {
                dialog->response(Gtk::RESPONSE_CANCEL);
                return false;
            }
            GtkTreeIter iter;
            gboolean more = gtk_tree_model_get_iter_first(view->get_model()->gobj(), &iter);
            int row = 0;
            bool ok = true;
            while (more && row < CitySession::kScenarioCount) {
                gchar *name = nullptr;
                gchar *goal = nullptr;
                gtk_tree_model_get(view->get_model()->gobj(), &iter, 1, &name, 2, &goal, -1);
                const CitySession::ScenarioDef &def = CitySession::scenario_def(row);
                const std::string shown_goal = goal == nullptr ? "" : goal;
                const std::string shown_name = name == nullptr ? "" : name;
                if (shown_goal != def.goal || shown_name.find(def.name) == std::string::npos ||
                    shown_goal.find("within") == std::string::npos) {
                    std::cerr << "row " << row << " '" << shown_name << "' goal '" << shown_goal << "'\n";
                    ok = false;
                }
                g_free(name);
                g_free(goal);
                ++row;
                more = gtk_tree_model_iter_next(view->get_model()->gobj(), &iter);
            }
            saw_goals = ok && row == CitySession::kScenarioCount;
            dialog->response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        return true;
    });
    if (hostile_review_window_probe(window, 9) != 1 || !saw_goals) {
        list_idle.disconnect();
        return fail(3, "the scenario list does not state each goal and deadline");
    }
    list_idle.disconnect();

    if (const int budget = budget_cases()) {
        return budget;
    }

    window.present();
    pump();
    if (hostile_review_window_probe(window, 8) != 1) {
        return fail(4, "clicking a power, water, or problem map did not move the city");
    }

    if (hostile_review_window_probe(window, 5) != 1) {
        return fail(5, "starting Dullsville did not show the goal and the years left");
    }

    if (hostile_review_window_probe(window, 7) != 1) {
        return fail(6, "Dismiss left the welcome line on screen");
    }
    const std::string flag = std::string(home) + "/.config/lunduke-city/welcome-dismissed";
    if (!file_exists(flag) || file_exists("/tmp/xfce4-must-not-be-written") ||
        flag.find("/xfce") != std::string::npos) {
        std::cerr << "flag '" << flag << "'\n";
        return fail(7, "the welcome dismissal was not stored under ~/.config/lunduke-city");
    }
    {
        AppWindow again;
        again.show_all();
        pump();
        const int bits = hostile_review_window_probe(again, 6);
        if ((bits & 4) != 0 || (bits & 1) == 0 || (bits & 16) == 0) {
            std::cerr << "second bits " << bits << "\n";
            return fail(8, "a later launch showed the welcome again or started the clock");
        }
        again.hide();
    }

    auto close_outcome = [&](int response, const char *expect) {
        bool closed = false;
        std::string secondary;
        sigc::connection idle = Glib::signal_idle().connect([&]() -> bool {
            for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
                auto *dialog = dynamic_cast<Gtk::MessageDialog *>(top);
                if (dialog == nullptr || !dialog->get_visible()) {
                    continue;
                }
                secondary = visible_label_blob(dialog);
                if (secondary.find("Scenario won") == std::string::npos &&
                    secondary.find("Scenario lost") == std::string::npos) {
                    continue;
                }
                dialog->response(response);
                closed = true;
                return false;
            }
            return true;
        });
        const int armed = hostile_review_window_probe(window, 4);
        for (int i = 0; i < 40 && !closed; ++i) {
            pump();
        }
        idle.disconnect();
        if (armed != 1 || !closed || secondary.find(expect) == std::string::npos ||
            secondary.find("paused for this announcement") == std::string::npos ||
            secondary.find("leaves the city paused") == std::string::npos) {
            std::cerr << "armed " << armed << " closed " << closed << " text '" << secondary << "'\n";
            return false;
        }
        return true;
    };

    if (!close_outcome(Gtk::RESPONSE_DELETE_EVENT, "The goal was met:")) {
        return fail(9, "could not close the win announcement");
    }
    if (hostile_review_window_probe(window, 11) != 10) {
        std::cerr << "after close " << hostile_review_window_probe(window, 11) << "\n";
        return fail(10, "closing the announcement started the clock");
    }

    if (!close_outcome(Gtk::RESPONSE_OK, "The goal was met:")) {
        return fail(11, "could not press Keep playing");
    }
    if (hostile_review_window_probe(window, 11) != 3) {
        std::cerr << "after keep " << hostile_review_window_probe(window, 11) << "\n";
        return fail(12, "Keep playing did not resume the previous speed");
    }

    int stage = 0;
    sigc::connection cancel_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *message = dynamic_cast<Gtk::MessageDialog *>(top);
            if (message != nullptr && message->get_visible()) {
                const std::string text = visible_label_blob(message);
                if (stage == 0 && (text.find("Scenario won") != std::string::npos ||
                                   text.find("Scenario lost") != std::string::npos)) {
                    // response() returns from dialog.run() after this idle
                    // finishes, so the save prompt is not opened underneath it.
                    message->response(Gtk::RESPONSE_ACCEPT);
                    stage = 1;
                    return true;
                }
                if (text.find("Save changes") != std::string::npos) {
                    message->response(Gtk::RESPONSE_REJECT);
                    return true;
                }
            }
            auto *dialog = dynamic_cast<Gtk::Dialog *>(top);
            if (dialog != nullptr && dialog->get_visible() && dialog->get_title() == "Play Scenario") {
                dialog->response(Gtk::RESPONSE_CANCEL);
                stage = 2;
                return false;
            }
        }
        return stage < 2;
    });
    if (hostile_review_window_probe(window, 4) != 1) {
        cancel_idle.disconnect();
        return fail(13, "could not open the announcement before cancelling the list");
    }
    for (int i = 0; i < 60 && stage < 2; ++i) {
        pump();
    }
    cancel_idle.disconnect();
    if (stage != 2 || hostile_review_window_probe(window, 11) != 10) {
        std::cerr << "stage " << stage << " clock " << hostile_review_window_probe(window, 11) << "\n";
        return fail(14, "cancelling the scenario list started the clock");
    }
    if (hostile_review_window_probe(window, 12) != 1) {
        return fail(15, "the next scenario did not keep the speed from before the pause");
    }

    window.hide();
    return 0;
}

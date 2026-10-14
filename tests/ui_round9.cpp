// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 9 window check: a minimized running city with Auto budget off still
// pauses for the tax-year budget, shows it on restore, and drops sound
// effects instead of queueing them (C-1).

#include "app_window.hpp"

#include <gtkmm/main.h>

#include <cstdlib>
#include <iostream>
#include <unistd.h>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

void pump()
{
    for (int i = 0; i < 12; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip or start its own server\n";
        return 1;
    }
    char home_template[] = "/tmp/lunduke-round9-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return fail(1, "could not make a home directory");
    }
    setenv("HOME", home, 1);
    const std::string config = std::string(home) + "/config";
    setenv("XDG_CONFIG_HOME", config.c_str(), 1);

    Gtk::Main kit(argc, argv);
    AppWindow window;
    window.show_all();
    pump();

    const int bits = round9_window_probe(window, 1);
    std::cout << "round9 bits " << bits << "\n";
    if ((bits & 1) == 0) {
        return fail(2, "a minimized city queued sound effects (C-1)");
    }
    if ((bits & 2) == 0 || (bits & 8) == 0) {
        return fail(3, "a minimized city did not pause for the tax-year budget (C-1)");
    }
    if ((bits & 4) == 0) {
        return fail(4, "the budget window opened while minimized");
    }
    if ((bits & 16) == 0) {
        return fail(5, "the budget window did not come up on restore (C-1)");
    }
    std::cout << "ui_round9 ok\n";
    return 0;
}

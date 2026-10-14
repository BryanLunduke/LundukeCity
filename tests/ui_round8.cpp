// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 8 window checks: a paused or minimized city stops refreshing (C-2),
// Pause and speeds have shown shortcuts (C-3), disasters ask first (C-4),
// and Quit asks to save after time has passed (C-1).

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
    char home_template[] = "/tmp/lunduke-round8-home-XXXXXX";
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

    const int running = round8_window_probe(window, 2);
    const int paused = round8_window_probe(window, 1);
    std::cout << "refreshes/s running=" << running << " paused=" << paused << "\n";
    if (running < 5) {
        return fail(2, "a running city stopped refreshing");
    }
    if (paused > 1) {
        return fail(3, "a paused city still refreshes with nothing changing (C-2)");
    }
    if (round8_window_probe(window, 3) != 1) {
        return fail(4, "a minimized city still refreshes, or stopped simulating (C-2)");
    }
    const int keys = round8_window_probe(window, 4);
    if (keys != (1 | 2 | 4 | 8 | 16)) {
        std::cerr << "key bits " << keys << "\n";
        return fail(5, "Pause and speed shortcuts are missing from the menu or do not work (C-3)");
    }
    if (round8_window_probe(window, 5) != 1) {
        return fail(6, "Cancel on the disaster confirm still started a disaster (C-4)");
    }
    if (round8_window_probe(window, 6) != 1) {
        return fail(7, "confirming a disaster did not start it (C-4)");
    }
    if (round8_window_probe(window, 7) != 1) {
        return fail(8, "Quit did not ask to save after time passed (C-1)");
    }
    std::cout << "ui_round8 ok\n";
    return 0;
}

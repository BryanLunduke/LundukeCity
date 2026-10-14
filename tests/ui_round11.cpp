// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 11 window check (C-2): the main window's minimum size fits a
// 1280x720 screen with a panel and title bar, and at GDK_SCALE=2 on a
// 1366x768 screen (683x384 logical). The default size is capped to the
// monitor's work area.

#include "app_window.hpp"

#include <gdkmm/display.h>
#include <gdkmm/monitor.h>
#include <gtkmm/main.h>

#include <cstdlib>
#include <iostream>
#include <unistd.h>

namespace {
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
    char home_template[] = "/tmp/lunduke-round11-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return 1;
    }
    setenv("HOME", home, 1);
    const std::string config = std::string(home) + "/config";
    setenv("XDG_CONFIG_HOME", config.c_str(), 1);

    Gtk::Main kit(argc, argv);
    AppWindow window;
    window.show_all();
    pump();

    Gtk::Requisition minimum;
    Gtk::Requisition natural;
    window.get_preferred_size(minimum, natural);
    int def_w = 0;
    int def_h = 0;
    window.get_default_size(def_w, def_h);
    Gdk::Rectangle area;
    Gdk::Display::get_default()->get_monitor(0)->get_workarea(area);
    std::cout << "minimum " << minimum.width << "x" << minimum.height << " default " << def_w << "x"
              << def_h << " workarea " << area.get_width() << "x" << area.get_height() << "\n";
    int rc = 0;
    // 683x384 logical at scale 2 on 1366x768, less a title bar.
    // (the real shrink is checked with xdotool under a window manager)
    if (minimum.width > 660 || minimum.height > 360) {
        std::cerr << "minimum size does not fit GDK_SCALE=2 on 1366x768 (C-2)\n";
        rc = 2;
    }
    if (def_w > area.get_width() || def_h > area.get_height()) {
        std::cerr << "default size is larger than the work area (C-2)\n";
        rc = 3;
    }
    return rc;
}

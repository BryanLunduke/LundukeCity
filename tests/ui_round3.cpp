// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Window layout and menu letters from the round-3 review (findings 15 and 16).

#include "app_window.hpp"

#include <gtkmm/container.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/scrolledwindow.h>

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

void collect(Gtk::Widget *widget, std::vector<std::string> &labels, Gtk::ScrolledWindow *&side)
{
    if (widget == nullptr) {
        return;
    }
    if (widget->get_name() == "side-panel-scroll") {
        if (auto *scrolled = dynamic_cast<Gtk::ScrolledWindow *>(widget)) {
            side = scrolled;
        }
    }
    if (auto *item = dynamic_cast<Gtk::MenuItem *>(widget)) {
        labels.push_back(item->get_label());
        if (Gtk::Menu *submenu = item->get_submenu()) {
            collect(static_cast<Gtk::Widget *>(submenu), labels, side);
        }
    }
    if (auto *container = dynamic_cast<Gtk::Container *>(widget)) {
        for (Gtk::Widget *child : container->get_children()) {
            collect(child, labels, side);
        }
    }
}

bool has_label(const std::vector<std::string> &labels, const char *text)
{
    for (const auto &label : labels) {
        if (label == text) {
            return true;
        }
    }
    return false;
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
    for (int i = 0; i < 8; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }

    int width = 0;
    int height = 0;
    window.get_size_request(width, height);
    std::vector<std::string> labels;
    Gtk::ScrolledWindow *side = nullptr;
    collect(window.get_child(), labels, side);

    int code = 0;
    if (width < 800 || height < 700) {
        std::cerr << "size request " << width << "x" << height << "\n";
        code = fail(1, "the window can still shrink small enough to clip the tools");
    } else if (side == nullptr) {
        code = fail(2, "the side panel is not in a scrolled window");
    } else {
        Gtk::PolicyType horizontal = Gtk::POLICY_ALWAYS;
        Gtk::PolicyType vertical = Gtk::POLICY_ALWAYS;
        side->get_policy(horizontal, vertical);
        if (vertical != Gtk::POLICY_AUTOMATIC) {
            code = fail(3, "the side panel does not scroll vertically");
        }
    }
    if (code == 0 && (!has_label(labels, "Save _City") || !has_label(labels, "Play _Scenario…") ||
                      !has_label(labels, "Auto b_ulldoze") || !has_label(labels, "M_onster") ||
                      !has_label(labels, "_Meltdown") || !has_label(labels, "_Pause") ||
                      !has_label(labels, "Slo_w") || !has_label(labels, "M_edium") ||
                      !has_label(labels, "_Fast"))) {
        for (const auto &label : labels) {
            std::cerr << "menu: " << label << "\n";
        }
        code = fail(4, "a menu item is still sharing a mnemonic");
    } else if (code == 0 && (has_label(labels, "_Save City") || has_label(labels, "Auto _bulldoze") ||
                             has_label(labels, "_Monster"))) {
        code = fail(5, "the old shared mnemonic is still on the menu");
    }

    window.hide();
    return code;
}

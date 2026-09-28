// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "app_window.hpp"

#include <gtkmm/application.h>
#include <gtkmm/window.h>

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv, "com.lunduke.LundukeCity");
    // WM / title-bar icon (xfwm4 etc.): desktop Icon= alone is not enough.
    Gtk::Window::set_default_icon_name("lunduke-city");
    AppWindow window;
    return app->run(window);
}

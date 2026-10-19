// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "app_window.hpp"

#include <giomm/file.h>
#include <glibmm/main.h>
#include <gtkmm/application.h>
#include <gtkmm/window.h>

#include <string>

namespace {

void present_for_open(AppWindow &window)
{
    window.present();
    // Map the window before a load error dialog, so the dialog is parented
    // and the process stays up instead of exiting.
    auto context = Glib::MainContext::get_default();
    for (int i = 0; i < 8 && context; ++i) {
        context->iteration(false);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv, "com.lunduke.LundukeCity", Gio::APPLICATION_HANDLES_OPEN);
    // WM / title-bar icon (xfwm4 etc.): desktop Icon= alone is not enough.
    Gtk::Window::set_default_icon_name("lunduke-city");
    AppWindow window;
    // A file open returns before the window is mapped. Without a hold the
    // process exits as soon as that handler returns, which is what made
    // `lunduke-city file.cty` quit before the city appeared. The hold is
    // released when the player closes the window.
    //
    // 0.9.5-2: hold only in the primary instance. startup runs only there;
    // a second launch just forwards its open/activate to the primary and
    // must then exit, not stay behind as a hidden process forever.
    bool held = false;
    app->signal_startup().connect([app, &held] {
        if (held) {
            return;
        }
        app->hold();
        held = true;
    });
    window.signal_hide().connect([app, &held] {
        if (!held) {
            return;
        }
        held = false;
        app->release();
    });
    app->signal_open().connect([&window](const Gtk::Application::type_vec_files &files, const Glib::ustring &) {
        present_for_open(window);
        // 0.9.5-2: request_open holds the file while a City dialog is up
        // and opens it after the dialog closes.
        if (files.empty() || !files[0]) {
            window.request_open({});
            return;
        }
        // A single-city window opens the first file. %f from the file
        // manager passes one path. A bad or missing file shows a dialog.
        window.request_open(files[0]->get_path());
    });
    return app->run(window);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// 0.9.5-2 C-1 (needs DISPLAY): a city opened from outside (command line,
// file manager, second launch) while the Save City dialog is up is held and
// opened only after the dialog closes, so the city under the dialog never
// changes and cityA.cty is not touched. Save City As refuses to write when
// the city was replaced while its dialog was up (binding check).

#include "app_window.hpp"
#include "city_session.hpp"

#include <glibmm/main.h>
#include <gtk/gtk.h>
#include <gtkmm/main.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iterator>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {
int g_fail = 0;
void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    std::fflush(stdout);
    if (!ok) {
        ++g_fail;
    }
}
bool pump_until(const std::function<bool()> &ready, int ms)
{
    const gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    for (;;) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
        if (ready()) return true;
        if (g_get_monotonic_time() >= end) return false;
        g_usleep(10000);
    }
}
std::string slurp(const std::string &p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
bool exists(const std::string &p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}
std::string title_of(GtkWindow *w)
{
    const char *t = gtk_window_get_title(w);
    return t != nullptr ? t : "";
}
std::string message_of(GtkWindow *w)
{
    if (!GTK_IS_MESSAGE_DIALOG(w)) return "";
    gchar *text = nullptr;
    g_object_get(G_OBJECT(w), "text", &text, nullptr);
    std::string out = text != nullptr ? text : "";
    g_free(text);
    return out;
}

// Calls step for each visible modal dialog every 30 ms while started.
struct Driver {
    std::function<void(GtkWindow *)> step;
    guint id = 0;
    void start()
    {
        id = g_timeout_add(
            30,
            +[](gpointer data) -> gboolean {
                auto *self = static_cast<Driver *>(data);
                GList *tops = gtk_window_list_toplevels();
                std::vector<GtkWindow *> dialogs;
                for (GList *it = tops; it != nullptr; it = it->next) {
                    GtkWindow *w = GTK_WINDOW(it->data);
                    if (GTK_IS_DIALOG(w) && gtk_widget_get_visible(GTK_WIDGET(w)) && gtk_window_get_modal(w)) {
                        dialogs.push_back(w);
                    }
                }
                g_list_free(tops);
                for (GtkWindow *w : dialogs) self->step(w);
                return G_SOURCE_CONTINUE;
            },
            this);
    }
    void stop()
    {
        if (id != 0) g_source_remove(id);
        id = 0;
    }
    ~Driver() { stop(); }
};
} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip\n";
        return 1;
    }
    char home_template[] = "/tmp/lunduke-0952-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return 1;
    }
    const std::string root = home;
    setenv("HOME", home, 1);
    setenv("XDG_CONFIG_HOME", (root + "/config").c_str(), 1);
    setenv("XDG_STATE_HOME", (root + "/state").c_str(), 1);
    setenv("XDG_DATA_HOME", (root + "/data").c_str(), 1);
    mkdir((root + "/data").c_str(), 0700);
    unsetenv("LUNDUKE_CITY_RECOVERY_ANSWER");
    unsetenv("LUNDUKE_CITY_SHOT_RECOVERY");

    const std::string city_a = root + "/cityA.cty";
    const std::string city_b = root + "/cityB.cty";
    {
        CitySession a;
        a.new_city("Harbor Town", 7);
        check(a.save_city_as(city_a), "fixture cityA.cty (Harbor Town)");
        CitySession b;
        b.new_city("Dullsville", 11);
        check(b.save_city_as(city_b), "fixture cityB.cty (Dullsville)");
    }
    const std::string original_a = slurp(city_a);

    int one = 1;
    Gtk::Main kit(one, argv);
    AppWindow window;
    window.show();
    // Let the start-up recovery check (450 ms) pass with nothing to offer.
    pump_until([] { return false; }, 700);
    check(window.open_city_file(city_a) == AppWindow::CityFileOpen::Loaded, "Harbor Town loaded");
    check(round0952_window_probe(window, 2, "Harbor Town") == 1, "Harbor Town is the city");

    // ---- held while Save City is up; cancelling leaves cityA.cty alone ----
    {
        bool seen = false;
        bool held = false;
        bool prompt_under_dialog = false;
        std::size_t queued = 0;
        Driver d;
        d.step = [&](GtkWindow *w) {
            if (message_of(w).find("Save changes") != std::string::npos) {
                prompt_under_dialog = true; // must not happen under the chooser
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CANCEL);
                return;
            }
            if (title_of(w) != "Save City") return;
            if (!seen) {
                seen = true;
                window.request_open(city_b); // what main.cpp's signal_open does
                // Let a few main-loop turns pass under the dialog.
                for (int i = 0; i < 20; ++i) {
                    while (Gtk::Main::events_pending()) Gtk::Main::iteration(false);
                    g_usleep(10000);
                }
                held = round0952_window_probe(window, 2, "Harbor Town") == 1;
                queued = window.pending_open_count();
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_CANCEL);
            }
        };
        d.start();
        round0952_window_probe(window, 1, "");
        d.stop();
        check(seen, "Save City dialog opened");
        check(held, "the city under the Save dialog stayed Harbor Town");
        check(queued == 1, "the outside open was queued");
        check(!prompt_under_dialog, "no unsaved-city question on top of the Save dialog");
        check(slurp(city_a) == original_a, "cityA.cty unchanged");
        // After the dialog the held open runs (answer Discard if asked).
        Driver after;
        after.step = [&](GtkWindow *w) {
            if (message_of(w).find("Save changes") != std::string::npos) {
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_REJECT);
            }
        };
        after.start();
        const bool loaded = pump_until(
            [&] { return round0952_window_probe(window, 2, "Dullsville") == 1 && window.pending_open_count() == 0; },
            5000);
        after.stop();
        check(loaded, "the held city opened after the Save dialog closed");
        check(slurp(city_a) == original_a, "cityA.cty still unchanged after the held open");
    }

    // ---- binding: a city replaced under Save City As is not written ----
    {
        check(window.open_city_file(city_a) == AppWindow::CityFileOpen::Loaded, "Harbor Town loaded again");
        const std::string target = root + "/bound.cty";
        bool seen = false;
        bool refused = false;
        int ticks = 0;
        Driver d;
        d.step = [&](GtkWindow *w) {
            const std::string msg = message_of(w);
            if (msg.find("Save changes") != std::string::npos) {
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_REJECT);
                return;
            }
            if (msg.find("city changed while the Save dialog") != std::string::npos) {
                refused = true;
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_OK);
                return;
            }
            if (title_of(w) != "Save City") return;
            if (!seen) {
                seen = true;
                window.open_city_file(city_b); // bypasses the queue on purpose
                // The dialog already points at cityA's folder (root). Do not
                // change folders here: GTK 3's path bar asserts when a folder
                // change cancels its first icon lookup.
                gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(w), "bound.cty");
                return;
            }
            // The chooser checks the name asynchronously; keep pressing Save.
            if (++ticks >= 5) {
                gtk_dialog_response(GTK_DIALOG(w), GTK_RESPONSE_ACCEPT);
            }
        };
        d.start();
        round0952_window_probe(window, 1, "");
        d.stop();
        check(seen, "binding: Save City dialog opened");
        check(round0952_window_probe(window, 2, "Dullsville") == 1, "binding: the city was replaced mid-dialog");
        check(refused, "binding: Save As said the city changed");
        check(!exists(target), "binding: nothing was written");
        check(slurp(city_a) == original_a, "binding: cityA.cty unchanged");
    }

    window.hide();
    pump_until([] { return false; }, 100);
    const std::string cmd = "rm -rf '" + root + "'";
    if (std::system(cmd.c_str()) != 0) {
        std::printf("cleanup failed %s\n", root.c_str());
    }
    std::printf("%d failure(s)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

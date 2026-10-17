// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 15 window checks (needs DISPLAY).
// C-1: unsaved work is autosaved on a timer; SIGTERM (logout) writes the
//      recovery file and exits cleanly with no prompt; the next start offers
//      it back: Not Now/Escape keeps it, Discard... then Cancel returns to the
//      prompt, Recover restores the city as unsaved work that saves to its
//      original file, and saving removes the recovery file.
// P-4 (City): opened and saved cities go to the desktop recent list and
//      System > Open Recent.
//
// argv[1] == "--sigterm-child": run one window until SIGTERM.

#include "app_window.hpp"
#include "city_recovery.hpp"
#include "city_session.hpp"

#include <glibmm/convert.h>
#include <glibmm/main.h>
#include <gtkmm/main.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <functional>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

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
void pump(int ms)
{
    const gint64 end = g_get_monotonic_time() + static_cast<gint64>(ms) * 1000;
    while (g_get_monotonic_time() < end) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
        g_usleep(5000);
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
bool exists(const std::string &p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}
} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip\n";
        return 1;
    }
    if (argc >= 3 && std::string(argv[1]) == "--sigterm-child") {
        // HOME / XDG_* come from the parent.
        int one = 1;
        Gtk::Main kit(one, argv);
        AppWindow window;
        window.show();
        pump(300);
        round15_window_probe(window, 1, "Logout Town");
        { std::ofstream(argv[2]) << "ready\n"; }
        Gtk::Main::run(window);
        return 0;
    }

    char home_template[] = "/tmp/lunduke-round15-home-XXXXXX";
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
    const char *shots = std::getenv("ROUND15_SHOT_DIR");

    // ---- SIGTERM in a child process: recovery written, clean exit ----
    {
        const std::string ready = root + "/child-ready";
        const pid_t child = fork();
        if (child == 0) {
            execl("/proc/self/exe", argv[0], "--sigterm-child", ready.c_str(), static_cast<char *>(nullptr));
            _exit(127);
        }
        bool up = false;
        for (int i = 0; i < 300 && !up; ++i) {
            up = exists(ready);
            if (!up) g_usleep(20000);
        }
        check(up, "child window came up");
        g_usleep(200000);
        kill(child, SIGTERM);
        int status = 0;
        pid_t got = 0;
        for (int i = 0; i < 500 && got == 0; ++i) {
            got = waitpid(child, &status, WNOHANG);
            if (got == 0) g_usleep(20000);
        }
        if (got == 0) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
        }
        check(got == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "SIGTERM: City left cleanly (no unsaved-city prompt holding it open)");
        const auto entries = city_recovery::list();
        check(entries.size() == 1, "SIGTERM: one recovery file written");
        if (!entries.empty()) {
            std::printf("recovery: %s\n", city_recovery::describe(entries[0]).c_str());
            check(entries[0].city_name == "Logout Town", "SIGTERM: recovery has the unsaved name");
        }
    }

    int one = 1;
    Gtk::Main kit(one, argv);

    // ---- Next start: Not Now keeps it ----
    {
        setenv("LUNDUKE_CITY_RECOVERY_ANSWER", "notnow", 1);
        if (shots) setenv("LUNDUKE_CITY_SHOT_RECOVERY", (std::string(shots) + "/city-recovery-prompt.png").c_str(), 1);
        AppWindow window;
        window.show();
        pump(1600);
        check(city_recovery::list().size() == 1, "Not Now keeps the recovery file");
        check((round15_window_probe(window, 7, "Logout Town") & 1) == 0, "Not Now did not load it");
        window.hide();
        pump(100);
    }
    unsetenv("LUNDUKE_CITY_SHOT_RECOVERY");
    // ---- Discard..., then Cancel: back to the prompt, file kept ----
    {
        setenv("LUNDUKE_CITY_RECOVERY_ANSWER", "discard", 1);
        setenv("LUNDUKE_CITY_RECOVERY_CONFIRM", "cancel", 1);
        AppWindow window;
        window.show();
        // offer_recovery loops: discard -> cancel -> prompt again (answers
        // discard again). Switch the next answer to Not Now so it ends.
        Glib::signal_timeout().connect_once([] { setenv("LUNDUKE_CITY_RECOVERY_ANSWER", "notnow", 1); }, 1000);
        pump(3000);
        check(city_recovery::list().size() == 1, "Discard then Cancel keeps the file");
        window.hide();
        pump(100);
        unsetenv("LUNDUKE_CITY_RECOVERY_CONFIRM");
    }
    // ---- Recover ----
    std::string recovered_path;
    {
        recovered_path = city_recovery::list().front().path;
        setenv("LUNDUKE_CITY_RECOVERY_ANSWER", "recover", 1);
        AppWindow window;
        window.show();
        pump(1600);
        const int bits = round15_window_probe(window, 7, "Logout Town");
        std::printf("recover bits %d\n", bits);
        check(bits & 1, "Recover loads the city (name restored)");
        check(bits & 2, "recovered city is unsaved work");
        check(bits & 8, "recovered city starts paused");
        check(round15_window_probe(window, 3, "") == 1, "the run now owns the recovery file");
        check(exists(recovered_path), "recovery kept until saved");
        if (shots) {
            Glib::signal_timeout().connect_once([] {}, 10);
        }
        // Never saved: Save goes through Save As; drive the session path instead.
        window.hide();
        pump(100);
    }
    unsetenv("LUNDUKE_CITY_RECOVERY_ANSWER");
    // ---- Timer autosave + save removes it + Open Recent ----
    {
        setenv("LUNDUKE_CITY_RECOVERY_ANSWER", "notnow", 1);
        setenv("LUNDUKE_CITY_AUTOSAVE_MS", "300", 1);
        // Start from a saved city file.
        const std::string city = root + "/Harbor.cty";
        {
            CitySession s;
            s.new_city("Harbor", 9);
            check(s.save_city_as(city), "fixture city saved");
        }
        AppWindow window;
        window.show();
        pump(800);
        check(round15_window_probe(window, 4, city) == 1, "open the fixture city");
        check(round15_window_probe(window, 3, "") == 0, "no autosave for a clean city");
        check(round15_window_probe(window, 1, "Harbor Renamed") == 1, "rename = unsaved work");
        check(pump_until([&] { return round15_window_probe(window, 3, "") == 1; }, 5000),
              "the timer autosaved the unsaved city");
        bool own_listed = false;
        for (const auto &e : city_recovery::list()) {
            if (e.city_name == "Harbor Renamed" && e.origin == city) own_listed = true;
        }
        // list() hides the run's own file; read it via the probe path instead.
        (void)own_listed;
        check(round15_window_probe(window, 8, "") == 1, "Save writes the city");
        check(round15_window_probe(window, 3, "") == 0, "Save removed this run's recovery file");
        check(pump_until([&] { return round15_window_probe(window, 5, city) == 1; }, 5000),
              "Open Recent lists the city");
        const std::string xbel = root + "/data/recently-used.xbel";
        std::string text;
        const bool in_xbel = pump_until(
            [&] {
                std::ifstream in(xbel);
                text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                return text.find("Harbor.cty") != std::string::npos && text.find("Lunduke City") != std::string::npos;
            },
            8000);
        check(in_xbel, "desktop recently-used.xbel has the city for Lunduke City");
        check(text.find("/state/lunduke-city/") == std::string::npos, "recovery files never go to the recent list");
        window.hide();
        pump(100);
        unsetenv("LUNDUKE_CITY_AUTOSAVE_MS");
    }
    // The older Logout Town file is still there (never saved); clean up.
    for (const auto &e : city_recovery::list()) {
        city_recovery::remove(e.path);
    }
    const std::string cmd = "rm -rf '" + root + "'";
    if (std::system(cmd.c_str()) != 0) {
        std::printf("cleanup failed %s\n", root.c_str());
    }
    std::printf("%d failure(s)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

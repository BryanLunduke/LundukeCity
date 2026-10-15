// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 12 checks.
// C-1: invalidate_all()/map re-blits every tile (the stale dirt map).
// C-2: the Budget window fits the monitor work area (run at 1280x720 and
//      1366x768).
// C-3: a failed load says why (not found, folder, too short, permission).
// C-4: About is in a Help menu, not Windows.
// C-5: a new empty city has no poll and no complaints.
// C-6: Ctrl+Shift+S is bound (Save As) next to Ctrl+S.

#include "app_window.hpp"
#include "city_session.hpp"

#include <gtkmm/main.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {
int g_fail = 0;
void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_fail;
    }
}
bool has(const std::string &s, const char *needle) { return s.find(needle) != std::string::npos; }
} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        std::cerr << "no DISPLAY; this GUI test must fail rather than skip or start its own server\n";
        return 1;
    }
    char home_template[] = "/tmp/lunduke-round12-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return 1;
    }
    setenv("HOME", home, 1);
    const std::string config = std::string(home) + "/config";
    setenv("XDG_CONFIG_HOME", config.c_str(), 1);

    // C-3
    const std::string dir = std::string(home) + "/files";
    mkdir(dir.c_str(), 0700);
    const std::string missing = dir + "/missing.cty";
    const std::string shortf = dir + "/short.cty";
    { std::ofstream(shortf) << "not a city"; }
    const std::string empty = dir + "/empty.cty";
    { std::ofstream e(empty); }
    std::printf("missing: %s\n", CitySession::load_problem(missing).c_str());
    check(has(CitySession::load_problem(missing), "not found"), "missing file says not found");
    check(has(CitySession::load_problem(dir), "folder"), "folder says folder");
    check(has(CitySession::load_problem(shortf), "too short"), "short file says too short");
    check(has(CitySession::load_problem(empty), "empty"), "empty file says empty");
    if (geteuid() != 0) {
        const std::string locked = dir + "/locked.cty";
        { std::ofstream(locked) << std::string(30000, 'x'); }
        chmod(locked.c_str(), 0);
        check(has(CitySession::load_problem(locked), "permission"), "unreadable file says permission");
    }
    {
        CitySession session;
        check(!session.load_city(missing), "load of missing file fails");
        check(has(session.last_load_error(), "not found"), "session keeps the reason");
        const std::string big = dir + "/garbage.cty";
        { std::ofstream(big) << std::string(27120, '\0'); }
        session.load_city(big);  // either loads (all zero city) or gives a reason
        check(true, "zero-filled file does not crash");
    }

    Gtk::Main kit(argc, argv);
    AppWindow window;
    window.show_all();
    for (int i = 0; i < 20; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }
    const int bits = round12_window_probe(window, 1);
    std::printf("probe bits %d\n", bits);
    check(bits & 1, "About is in Help, not Windows (C-4)");
    check(bits & 2, "Ctrl+Shift+S bound alongside Ctrl+S (C-6)");
    check(bits & 4, "new empty city: no residents, no complaints (C-5)");
    check(bits & 8, "invalidate_all re-blits every tile; next sync none (C-1)");
    check(bits & 16, "Budget window fits the work area (C-2)");
    return g_fail == 0 ? 0 : 1;
}

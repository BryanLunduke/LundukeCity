// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 15 checks without a display.
// C-1: an autosave (write_recovery / saveFile commitPending=false) writes a
//      loadable city but leaves the live city's unsaved state, save path and
//      waiting tax year alone; load_recovery brings it back as unsaved work
//      that saves to the original file; the recovery folder helpers list,
//      describe and remove files.
// C-2: saving through a symlink replaces the real file and keeps the link;
//      a dangling link is refused with a Save City As hint.
// C-3: an existing file keeps its group; stale ".<name>.save-XXXXXX" files
//      older than a day are removed at save time, others are kept.

#include "city_recovery.hpp"
#include "city_session.hpp"
#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

namespace {
int g_fail = 0;
void check(bool ok, const char *what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_fail;
    }
}
std::string slurp(const std::string &p)
{
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
bool is_link(const std::string &p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0 && S_ISLNK(st.st_mode);
}
bool exists(const std::string &p)
{
    struct stat st;
    return lstat(p.c_str(), &st) == 0;
}
void make_old(const std::string &p, long age)
{
    { std::ofstream(p) << "partial"; }
    struct timeval tv[2];
    gettimeofday(&tv[0], nullptr);
    tv[0].tv_sec -= age;
    tv[1] = tv[0];
    utimes(p.c_str(), tv);
}
} // namespace

int main()
{
    char tmpl[] = "/tmp/lunduke-r15-XXXXXX";
    if (!mkdtemp(tmpl)) {
        return 1;
    }
    const std::string root = tmpl;
    setenv("HOME", root.c_str(), 1);
    const std::string state = root + "/state";
    setenv("XDG_STATE_HOME", state.c_str(), 1);
    const std::string real_dir = root + "/nas";
    const std::string link_dir = root + "/Cities";
    mkdir(real_dir.c_str(), 0755);
    mkdir(link_dir.c_str(), 0755);

    Micropolis sim;
    sim.generateSomeCity(15);
    sim.setCleanCityName("Linked Town");

    // ---- C-2: symlink ----
    {
        const std::string real = real_dir + "/main.cty";
        check(sim.saveFile(real.c_str()), "save the real file");
        chmod(real.c_str(), 0640);
        const std::string before = slurp(real);
        const std::string link = link_dir + "/main.cty";
        symlink(real.c_str(), link.c_str());
        sim.map[2][2] = static_cast<unsigned short>(sim.map[2][2] ^ 1);
        const bool ok = sim.saveCityAs(link.c_str());
        std::printf("save via link: ok=%d detail='%s'\n", ok ? 1 : 0, sim.saveErrorDetail.c_str());
        check(ok, "Save through a symlink succeeds (was \"Is a symlink\")");
        check(is_link(link), "the link is still a link");
        check(slurp(real) != before && slurp(real).size() == before.size(), "the real file was replaced");
        check(sim.cityFileName == link, "the engine keeps the link as the city's file name");
        struct stat st;
        check(stat(real.c_str(), &st) == 0 && (st.st_mode & 0777) == 0640, "the real file keeps 0640");
        int temps = 0;
        for (const std::string d : {real_dir, link_dir}) {
            if (DIR *dp = opendir(d.c_str())) {
                while (dirent *e = readdir(dp)) {
                    if (std::strstr(e->d_name, ".save-") != nullptr) ++temps;
                }
                closedir(dp);
            }
        }
        check(temps == 0, "no temp file left in either folder");
        Micropolis back;
        check(back.loadCity(link.c_str()) && back.cityName == "Linked Town", "the saved city loads through the link");

        const std::string dangling = link_dir + "/gone.cty";
        symlink("/nonexistent/lunduke/gone.cty", dangling.c_str());
        const bool bad = sim.saveFile(dangling.c_str());
        std::printf("dangling: ok=%d detail='%s'\n", bad ? 1 : 0, sim.saveErrorDetail.c_str());
        check(!bad, "a dangling link is refused");
        check(sim.saveErrorDetail.find("Save City As") != std::string::npos, "the refusal suggests Save City As");
        check(is_link(dangling), "the dangling link is left alone");
    }

    // ---- C-3: group kept ----
    {
        gid_t groups[64];
        const int n = getgroups(64, groups);
        gid_t other = (gid_t)-1;
        for (int i = 0; i < n; ++i) {
            if (groups[i] != getegid()) {
                other = groups[i];
                break;
            }
        }
        if (other == (gid_t)-1) {
            std::printf("SKIP: no supplementary group to test the group copy\n");
        } else {
            const std::string shared = real_dir + "/shared.cty";
            check(sim.saveFile(shared.c_str()), "save shared city");
            check(chown(shared.c_str(), (uid_t)-1, other) == 0, "give it a shared group");
            chmod(shared.c_str(), 0660);
            check(sim.saveFile(shared.c_str()), "save over the shared city");
            struct stat st;
            check(stat(shared.c_str(), &st) == 0 && st.st_gid == other, "the file keeps its group (was the saver's)");
            check((st.st_mode & 0777) == 0660, "and its mode");
        }
    }

    // ---- C-3: stale temps ----
    {
        const std::string d = root + "/stale";
        mkdir(d.c_str(), 0755);
        const std::string old_tmp = d + "/.town.cty.save-Ab12Cd";
        const std::string young_tmp = d + "/.town.cty.save-Zz99Yy";
        const std::string other_tmp = d + "/.other.cty.save-Ab12Cd";
        const std::string user_file = d + "/.town.cty.save-notes.txt";
        make_old(old_tmp, 2 * 24 * 3600);
        make_old(young_tmp, 600);
        make_old(other_tmp, 2 * 24 * 3600);
        make_old(user_file, 2 * 24 * 3600);
        const std::string link_tmp = d + "/.town.cty.save-Lk00Lk";
        symlink(user_file.c_str(), link_tmp.c_str());
        const std::string town = d + "/town.cty";
        check(sim.saveFile(town.c_str()), "save town.cty");
        check(!exists(old_tmp), "a day-old leftover temp of town.cty is removed");
        check(exists(young_tmp), "a recent temp is kept (a save may be in progress)");
        check(exists(other_tmp), "another city's temp is kept");
        check(exists(user_file), "a user file with a similar name is kept");
        check(is_link(link_tmp), "a temp-named symlink is not followed or removed");
    }

    // ---- C-1: autosave does not collect a waiting tax year ----
    {
        Micropolis b;
        b.generateSomeCity(7);
        b.autoBudget = false;
        b.taxFund = 2000;
        b.roadFund = 500;
        b.policeFund = 0;
        b.fireFund = 0;
        b.roadPercent = 1.0f;
        b.policePercent = 0.0f;
        b.firePercent = 0.0f;
        b.setFunds(5000);
        b.doBudgetNow(false);
        check(b.budgetAwaitingAccept && static_cast<long>(b.totalFunds) == 5000, "tax year waiting");
        const std::string rec = root + "/auto.cty";
        b.cityFileName = "keep-name";
        check(b.saveFile(rec.c_str(), false), "autosave write");
        check(b.budgetAwaitingAccept && static_cast<long>(b.totalFunds) == 5000,
              "autosave left the tax year waiting and funds alone");
        check(b.cityFileName == "keep-name", "autosave did not change the engine file name");
        Micropolis r;
        check(r.loadCity(rec.c_str()), "the autosave loads");
        check(b.saveFile(rec.c_str(), true) && !b.budgetAwaitingAccept, "a real save still collects it");
    }

    // ---- C-1: session write_recovery / load_recovery ----
    {
        CitySession s;
        s.new_city("Harbor Town", 42);
        const std::string origin = root + "/harbor.cty";
        check(s.save_city_as(origin), "session save");
        check(!s.needs_save_prompt(), "clean after save");
        s.rename_city("Harbor Bay");
        check(s.needs_save_prompt(), "rename is unsaved work");
        const std::string rec = city_recovery::new_recovery_path();
        check(!rec.empty() && rec.compare(0, state.size(), state) == 0, "recovery path is under XDG_STATE_HOME");
        struct stat dst;
        check(stat((state + "/lunduke-city").c_str(), &dst) == 0 && (dst.st_mode & 0777) == 0700,
              "recovery folder is 0700");
        const std::string message_before = s.message();
        check(s.write_recovery(rec), "write_recovery");
        check(city_recovery::write_info(rec, s.city_name(), s.save_path(), s.date_text()), "write_info");
        check(s.needs_save_prompt(), "still unsaved after an autosave");
        check(s.save_path() == origin, "save path unchanged by an autosave");
        check(s.message() == message_before, "no notice from an autosave");

        const auto entries = city_recovery::list();
        check(entries.size() == 1, "one recovery entry listed");
        if (!entries.empty()) {
            const std::string desc = city_recovery::describe(entries[0]);
            std::printf("describe: %s\n", desc.c_str());
            check(entries[0].city_name == "Harbor Bay", "entry has the city name");
            check(entries[0].origin == origin, "entry has the original file");
            check(desc.find("Harbor Bay (harbor.cty)") != std::string::npos && desc.find("autosaved") != std::string::npos,
                  "description names the city, its file and the autosave time");
        }
        check(city_recovery::list(rec).empty(), "the run's own file is not offered to itself");

        CitySession t;
        t.new_city("Other", 1);
        check(t.load_recovery(rec, origin), "load_recovery");
        check(t.city_name() == "Harbor Bay", "recovered city name");
        check(t.needs_save_prompt(), "recovered city counts as unsaved");
        check(t.save_path() == origin, "Save goes to the original file");
        check(t.save_city_as(t.save_path()) && !t.needs_save_prompt(), "saving the recovered city");
        Micropolis check_back;
        check(check_back.loadCity(origin.c_str()) && check_back.cityName == "Harbor Bay", "original file now has the work");

        // Never-saved city: no origin.
        CitySession u;
        u.new_city("Fresh", 3);
        const std::string rec2 = city_recovery::new_recovery_path() + "-b.cty";
        check(u.write_recovery(rec2), "write_recovery for a never-saved city");
        city_recovery::write_info(rec2, u.city_name(), u.save_path(), u.date_text());
        const auto e2 = city_recovery::read_entry(rec2);
        check(city_recovery::describe(e2).find("(never saved)") != std::string::npos, "never-saved description");
        CitySession v;
        check(v.load_recovery(rec2, "") && v.save_path().empty() && v.needs_save_prompt(),
              "never-saved recovery opens with no path (Save asks where)");

        city_recovery::remove(rec);
        city_recovery::remove(rec2);
        check(!exists(rec) && !exists(city_recovery::info_path(rec)), "remove deletes the file and its sidecar");
        check(city_recovery::list().empty(), "nothing left to offer");

        // A symlinked recovery folder is refused.
        const std::string evil_state = root + "/evil-state";
        mkdir(evil_state.c_str(), 0700);
        symlink(real_dir.c_str(), (evil_state + "/lunduke-city").c_str());
        setenv("XDG_STATE_HOME", evil_state.c_str(), 1);
        check(city_recovery::dir(true).empty(), "a symlinked recovery folder is not used");
        setenv("XDG_STATE_HOME", state.c_str(), 1);
    }

    const std::string cmd = "rm -rf '" + root + "'";
    if (std::system(cmd.c_str()) != 0) {
        std::printf("cleanup failed: %s\n", root.c_str());
    }
    std::printf("%d failure(s)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

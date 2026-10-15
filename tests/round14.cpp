// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 14 engine checks.
// C-1: saving keeps the existing file's permissions (0600 stays 0600) and a
//      new file gets 0666 minus the umask.
// C-2: saving never touches a user's own "<city>.tmp" and leaves no temp
//      files behind.

#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static int fail(int code, const char *msg)
{
    std::cerr << "round14: " << msg << "\n";
    return code;
}

static mode_t mode_of(const std::string &p)
{
    struct stat st;
    if (stat(p.c_str(), &st) != 0) {
        return (mode_t)-1;
    }
    return st.st_mode & 07777;
}

static int count_entries(const std::string &dir)
{
    int n = 0;
    DIR *d = opendir(dir.c_str());
    if (!d) return -1;
    while (struct dirent *e = readdir(d)) {
        std::string name = e->d_name;
        if (name != "." && name != "..") ++n;
    }
    closedir(d);
    return n;
}

int main()
{
    char tmpl[] = "/tmp/lunduke-r14-XXXXXX";
    if (!mkdtemp(tmpl)) return fail(1, "mkdtemp");
    const std::string dir = tmpl;
    const std::string city = dir + "/priv.cty";
    const std::string user_tmp = city + ".tmp";
    int rc = 0;

    Micropolis sim;
    sim.generateSomeCity(14);

    // New file: umask 027 -> 0640.
    const mode_t old_mask = umask(027);
    if (!sim.saveFile(city.c_str())) rc = fail(2, "first save failed");
    else if (mode_of(city) != 0640) rc = fail(3, "new file did not respect the umask");
    umask(old_mask);

    // Existing 0600 file stays 0600.
    if (rc == 0) {
        chmod(city.c_str(), 0600);
        { std::ofstream(user_tmp) << "keep"; }
        if (!sim.saveFile(city.c_str())) rc = fail(4, "second save failed");
        else if (mode_of(city) != 0600) rc = fail(5, "save changed a 0600 file's mode");
    }
    // The user's .tmp file is untouched.
    if (rc == 0) {
        std::ifstream in(user_tmp);
        std::stringstream ss; ss << in.rdbuf();
        if (ss.str() != "keep") rc = fail(6, "save touched <city>.tmp");
    }
    // Only priv.cty and priv.cty.tmp remain: no leftover temp file.
    if (rc == 0 && count_entries(dir) != 2) rc = fail(7, "a temp file was left behind");

    // A 0640 file stays 0640 after a save with a different umask.
    if (rc == 0) {
        chmod(city.c_str(), 0640);
        const mode_t m = umask(077);
        const bool ok = sim.saveFile(city.c_str());
        umask(m);
        if (!ok || mode_of(city) != 0640) rc = fail(8, "save did not keep 0640");
    }

    std::remove(user_tmp.c_str());
    std::remove(city.c_str());
    rmdir(dir.c_str());
    if (rc == 0) std::cout << "round14 ok\n";
    return rc;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 11 engine check (C-1): saving onto a full disk must report ENOSPC
// ("No space left on device"), not EIO, and must leave no file behind.
// LUNDUKE_CITY_FULL_DIR names a directory on a filesystem that is full;
// without it the test skips (exit 77).

#include "micropolis.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>

int main()
{
    const char *dir = std::getenv("LUNDUKE_CITY_FULL_DIR");
    if (dir == nullptr || dir[0] == '\0') {
        std::cout << "LUNDUKE_CITY_FULL_DIR not set; skipping\n";
        return 77;
    }
    const std::string path = std::string(dir) + "/city.cty";
    Micropolis sim;
    sim.generateSomeCity(7);
    sim.saveErrno = 0;
    if (sim.saveFile(path.c_str())) {
        std::cerr << "save onto a full disk succeeded; is the directory full?\n";
        return 1;
    }
    std::cout << "saveErrno " << sim.saveErrno << " (" << std::strerror(sim.saveErrno) << ")\n";
    if (sim.saveErrno != ENOSPC) {
        std::cerr << "full disk was not reported as ENOSPC\n";
        return 2;
    }
    struct stat st;
    if (stat(path.c_str(), &st) == 0 || stat((path + ".tmp").c_str(), &st) == 0) {
        std::cerr << "a partial file was left behind\n";
        return 3;
    }
    return 0;
}

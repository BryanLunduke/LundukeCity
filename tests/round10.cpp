// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 10 engine check (C-1): a crafted save with a stadium, coal plant,
// airport or nuclear plant centre on the map edge must not make the
// simulation write outside the map. Loading strips ZONEBIT from zone centres
// whose footprint leaves the map; the footprint writers are bounds-checked
// too. Run under ASan/UBSan to catch any out-of-bounds write.

#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

struct Spot {
    int x;
    int y;
    MapTile tile;
    bool valid;
};

const Spot kSpots[] = {
    {WORLD_W - 1, 40, STADIUM, false},
    {WORLD_W - 2, 60, FULLSTADIUM, false},
    {0, 20, STADIUM, false},
    {WORLD_W - 1, 0, POWERPLANT, false},
    {WORLD_W - 2, 30, POWERPLANT, false},
    {50, WORLD_H - 1, NUCLEAR, false},
    {WORLD_W - 3, 70, AIRPORT, false},
    {0, 80, RESBASE + 4, false},
    {30, WORLD_H - 1, FIRESTATION, false},
    {60, 50, STADIUM, true},
    {WORLD_W - 3, 90, POWERPLANT, true},
    {WORLD_W - 2, 20, FIRESTATION, true},
};

void place(Micropolis &sim)
{
    for (const Spot &s : kSpots) {
        sim.map[s.x][s.y] = s.tile | ZONEBIT | PWRBIT | CONDBIT;
    }
}

} // namespace

int main()
{
    char dir_template[] = "/tmp/lunduke-round10-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (dir == nullptr) {
        return fail(1, "could not make a scratch directory");
    }
    const std::string path = std::string(dir) + "/edge.cty";
    {
        Micropolis sim;
        sim.generateSomeCity(42);
        place(sim);
        if (!sim.saveFile(path.c_str())) {
            return fail(2, "could not save the crafted city");
        }
    }

    // 1. Load sanitising: edge zone centres lose ZONEBIT, valid ones keep it.
    Micropolis sim;
    if (!sim.loadFile(path.c_str())) {
        return fail(3, "the crafted city did not load");
    }
    for (const Spot &s : kSpots) {
        const bool zone = (sim.map[s.x][s.y] & ZONEBIT) != 0;
        if (zone != s.valid) {
            std::cerr << "tile " << s.tile << " at " << s.x << "," << s.y
                      << (s.valid ? " lost" : " kept") << " ZONEBIT\n";
            return fail(4, "load did not sanitise edge-placed zone centres (C-1)");
        }
    }
    sim.setSpeed(3);
    sim.setPasses(1);
    for (int i = 0; i < 2000; ++i) {
        sim.simTick();
    }

    // 2. Bounds checks in the footprint writers themselves: put the edge
    // centres back after load (as a bug elsewhere might) and simulate.
    for (int round = 0; round < 3; ++round) {
        place(sim);
        for (int i = 0; i < 400; ++i) {
            sim.simTick();
        }
    }

    std::remove(path.c_str());
    rmdir(dir);
    std::cout << "round10 ok\n";
    return 0;
}

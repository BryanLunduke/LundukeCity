// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless check that the vendored engine can create a city, place a road,
// and tick. No UI toolkit.

#include "micropolis.h"

#include <cstdio>

int main()
{
    Micropolis sim;
    if (sim.totalFunds != 20000) {
        std::fprintf(stderr, "expected starting funds 20000, got %ld\n",
                     static_cast<long>(sim.totalFunds));
        return 1;
    }

    sim.generateSomeCity(12345);
    if (sim.cityYear != 1900) {
        std::fprintf(stderr, "expected year 1900, got %ld\n", static_cast<long>(sim.cityYear));
        return 2;
    }

    bool placed = false;
    for (int y = 0; y < WORLD_H && !placed; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            if ((sim.map[x][y] & LOMASK) != DIRT) {
                continue;
            }
            const long before = static_cast<long>(sim.totalFunds);
            if (sim.doTool(TOOL_ROAD, static_cast<short>(x), static_cast<short>(y)) == TOOLRESULT_OK) {
                if (sim.totalFunds != before - 10) {
                    std::fprintf(stderr, "road cost mismatch %ld -> %ld\n", before,
                                 static_cast<long>(sim.totalFunds));
                    return 3;
                }
                placed = true;
            }
        }
    }
    if (!placed) {
        std::fprintf(stderr, "could not place a road on dirt\n");
        return 4;
    }

    sim.setSpeed(3);
    sim.simTick();
    return 0;
}

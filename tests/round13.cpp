// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 13 engine and geometry checks.
// C-1: a random file of the right size, negative funds, an out-of-range date
//      or unknown tile values are rejected with a reason, and the live city is
//      untouched. Real saves and every shipped scenario still load.
// C-2: window_fit::clamp_origin keeps the frame inside a 768 px work area.

#include "micropolis.h"
#include "window_fit.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <unistd.h>

namespace {

int fail(int code, const std::string &message)
{
    std::cerr << message << "\n";
    return code;
}

std::vector<unsigned char> slurp(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void spit(const std::string &path, const std::vector<unsigned char> &d)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(d.data()), static_cast<std::streamsize>(d.size()));
}

// miscHist starts after six 480-byte histories; the map after 240 more.
const std::size_t kMisc = 6 * 480;
const std::size_t kMap = kMisc + 240;

void put_be16(std::vector<unsigned char> &d, std::size_t off, unsigned v)
{
    d[off] = static_cast<unsigned char>((v >> 8) & 0xff);
    d[off + 1] = static_cast<unsigned char>(v & 0xff);
}

// Mac long in miscHist[slot..slot+1]: high word first.
void put_long(std::vector<unsigned char> &d, int slot, long v)
{
    const unsigned bits = static_cast<unsigned>(static_cast<int>(v));
    put_be16(d, kMisc + 2 * slot, bits >> 16);
    put_be16(d, kMisc + 2 * slot + 2, bits & 0xffff);
}

} // namespace

int main()
{
    char dir_template[] = "/tmp/lunduke-round13-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (dir == nullptr) {
        return fail(1, "could not make a scratch directory");
    }
    const std::string good = std::string(dir) + "/good.cty";
    {
        Micropolis sim;
        sim.generateSomeCity(7);
        sim.setFunds(123456);
        if (!sim.saveFile(good.c_str())) {
            return fail(2, "could not save a city");
        }
    }
    Micropolis sim;
    if (!sim.loadFile(good.c_str()) || !sim.loadErrorDetail.empty()) {
        return fail(3, "a real save was rejected: " + sim.loadErrorDetail);
    }
    if (sim.totalFunds != 123456) {
        return fail(4, "funds did not round-trip");
    }
    const auto base = slurp(good);
    const MapValue before = sim.map[10][10];

    struct Case {
        const char *name;
        std::vector<unsigned char> data;
        const char *expect;
    };
    std::vector<Case> cases;
    {
        std::mt19937 rng(1313);
        std::vector<unsigned char> r(29338);
        for (auto &b : r) {
            b = static_cast<unsigned char>(rng());
        }
        cases.push_back({"random 29338 bytes", r, ""});
    }
    for (unsigned seed = 1; seed <= 200; ++seed) {
        std::mt19937 rng(seed);
        std::vector<unsigned char> r(27120 + (rng() % 4000));
        for (auto &b : r) {
            b = static_cast<unsigned char>(rng());
        }
        cases.push_back({"random fuzz", r, ""});
    }
    {
        // A classic-only file (no wide-funds trailer), like older saves.
        std::vector<unsigned char> d(base.begin(), base.begin() + 27120);
        put_long(d, 50, -1995955278);
        cases.push_back({"funds -1,995,955,278", d, "funds"});
    }
    {
        auto d = base;
        put_long(d, 8, (9999 - 1900) * 48 + 1);
        cases.push_back({"year past 9999", d, "date"});
    }
    {
        // Classic-only (original Micropolis) files keep the old clamp for a
        // far-future date, but a negative date is still refused.
        std::vector<unsigned char> d(base.begin(), base.begin() + 27120);
        put_long(d, 8, (9999 - 1900) * 48 + 100);
        const std::string p = std::string(dir) + "/classic.cty";
        spit(p, d);
        Micropolis classic;
        if (!classic.loadFile(p.c_str()) || classic.cityTime != (9999 - 1900) * 48) {
            return fail(14, "a classic file with a far-future date was not clamped");
        }
        std::remove(p.c_str());
    }
    {
        auto d = base;
        put_long(d, 8, -5);
        cases.push_back({"negative date", d, "date"});
    }
    {
        auto d = base;
        put_be16(d, kMap + 2 * 500, 1023);
        cases.push_back({"unknown tile", d, "unknown tile"});
    }
    int n = 0;
    for (const Case &c : cases) {
        const std::string p = std::string(dir) + "/bad" + std::to_string(n++) + ".cty";
        spit(p, c.data);
        if (sim.loadFile(p.c_str())) {
            return fail(10, std::string("garbage loaded: ") + c.name);
        }
        if (sim.loadErrorDetail.empty() || sim.loadErrorDetail.find(c.expect) == std::string::npos) {
            return fail(11, std::string("no or wrong reason for ") + c.name + ": " + sim.loadErrorDetail);
        }
        if (sim.map[10][10] != before || sim.totalFunds != 123456) {
            return fail(12, std::string("rejected load changed the live city: ") + c.name);
        }
        std::remove(p.c_str());
    }
    {
        // Ordinary debt in a classic-only file still loads.
        std::vector<unsigned char> d(base.begin(), base.begin() + 27120);
        put_long(d, 50, -5000);
        const std::string p = std::string(dir) + "/debt.cty";
        spit(p, d);
        Micropolis debt;
        if (!debt.loadFile(p.c_str()) || debt.totalFunds != -5000) {
            return fail(13, "a city in ordinary debt was rejected: " + debt.loadErrorDetail);
        }
        std::remove(p.c_str());
    }
    std::cout << "rejected " << cases.size() << " bad files; e.g. \"" << sim.loadErrorDetail << "\"\n";

    // Every shipped scenario still loads.
    Micropolis scen;
    scen.resourceDir = LUNDUKE_CITY_ASSET_DIR "/res";
    for (int s = SC_DULLSVILLE; s < SC_COUNT; ++s) {
        if (!scen.loadScenario(static_cast<Scenario>(s))) {
            return fail(20, "scenario " + std::to_string(s) + " rejected: " + scen.loadErrorDetail);
        }
    }

    // C-2: Budget 560x420 that the WM put at y=372 on 1366x768.
    int x = 400;
    int y = 372;
    window_fit::clamp_origin(x, y, 560, 420, 0, 0, 1366, 768);
    if (y + window_fit::kFrameTop + 420 + window_fit::kFrameBottom > 768 || y < 0 || x != 400) {
        return fail(30, "budget not clamped: y=" + std::to_string(y));
    }
    x = 100;
    y = 58;
    window_fit::clamp_origin(x, y, 1100, 720, 0, 0, 1366, 768);
    if (y != 0) {
        return fail(31, "main window not clamped: y=" + std::to_string(y));
    }
    x = -50;
    y = -20;
    window_fit::clamp_origin(x, y, 300, 200, 0, 27, 1280, 693);
    if (x != 0 || y != 27) {
        return fail(32, "top-left not clamped");
    }

    std::remove(good.c_str());
    rmdir(dir);
    std::cout << "round13 ok\n";
    return 0;
}

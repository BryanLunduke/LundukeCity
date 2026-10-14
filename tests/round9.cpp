// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 9 engine check: a corrupted wide trailer cannot load absurd 64-bit
// population or funds (C-2), and the city still simulates afterwards.

#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

void put_le64(std::vector<unsigned char> &d, std::size_t at, long long v)
{
    for (int i = 0; i < 8; ++i) {
        d[at + i] = static_cast<unsigned char>((static_cast<unsigned long long>(v) >> (8 * i)) & 0xff);
    }
}

} // namespace

int main()
{
    char dir_template[] = "/tmp/lunduke-round9-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (dir == nullptr) {
        return fail(1, "could not make a scratch directory");
    }
    const std::string base = std::string(dir) + "/base.cty";
    const std::string bad = std::string(dir) + "/wide.cty";
    {
        Micropolis sim;
        sim.generateSomeCity(7);
        if (!sim.saveFile(base.c_str())) {
            return fail(2, "could not save the city");
        }
    }
    std::ifstream in(base, std::ios::binary);
    std::vector<unsigned char> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::size_t at = std::string::npos;
    for (std::size_t i = 0; i + 4 <= d.size(); ++i) {
        if (std::memcmp(&d[i], "LCW1", 4) == 0) {
            at = i;
        }
    }
    if (at == std::string::npos || at + 8 + 32 > d.size()) {
        return fail(3, "saved city has no wide trailer");
    }
    // Funds then census (res, com, ind): huge, and negative.
    put_le64(d, at + 8, 0x7fffffffffffff00LL);
    put_le64(d, at + 16, 0x7fffffffffffff00LL);
    put_le64(d, at + 24, -0x7fffffffffffff00LL);
    put_le64(d, at + 32, 0x4000000000000000LL);
    {
        std::ofstream out(bad, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char *>(d.data()), static_cast<std::streamsize>(d.size()));
    }
    Micropolis sim;
    if (!sim.loadFile(bad.c_str())) {
        return fail(4, "the corrupted city did not load");
    }
    std::cout << "funds=" << sim.totalFunds << " res=" << sim.resPop << " com=" << sim.comPop
              << " ind=" << sim.indPop << "\n";
    const long long kMaxPop = 10000000LL;
    if (sim.totalFunds > 1000000000000LL || sim.totalFunds < -1000000000000LL) {
        return fail(5, "funds were not clamped on load (C-2)");
    }
    if (sim.resPop < 0 || sim.resPop > kMaxPop || sim.comPop < 0 || sim.comPop > kMaxPop ||
        sim.indPop < 0 || sim.indPop > kMaxPop) {
        return fail(6, "population was not clamped on load (C-2)");
    }
    sim.setSpeed(3);
    sim.setPasses(1);
    for (int i = 0; i < 3000; ++i) {
        sim.simTick();
    }
    std::remove(base.c_str());
    std::remove(bad.c_str());
    rmdir(dir);
    std::cout << "round9 ok\n";
    return 0;
}

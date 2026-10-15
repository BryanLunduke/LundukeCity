// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 8 session and engine checks: time passing is unsaved work (C-1),
// the engine starts with defined history state and range-checks enum
// fields from a file (C-5), and a corrupted save cannot jump the calendar
// to a six-digit year (C-6).

#include "city_session.hpp"

#include "micropolis.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

std::vector<unsigned char> read_all(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_all(const std::string &path, const std::vector<unsigned char> &data)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
}

void put_be16(std::vector<unsigned char> &data, std::size_t at, unsigned value)
{
    data[at] = static_cast<unsigned char>((value >> 8) & 0xff);
    data[at + 1] = static_cast<unsigned char>(value & 0xff);
}

} // namespace

int main()
{
    char dir_template[] = "/tmp/lunduke-round8-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (dir == nullptr) {
        return fail(1, "could not make a scratch directory");
    }
    const std::string city = std::string(dir) + "/time.cty";

    // C-1: save, let years pass with no edits, and the city is unsaved.
    {
        CitySession session;
        session.new_city("Time Town", 7);
        if (!session.save_city_as(city)) {
            return fail(2, "could not save the city");
        }
        if (session.needs_save_prompt()) {
            return fail(3, "a just-saved city asks to be saved");
        }
        session.set_speed(3);
        const std::string before = session.date_text();
        for (int i = 0; i < 4000; ++i) {
            session.tick();
        }
        if (session.date_text() == before) {
            return fail(4, "the simulated date did not move");
        }
        if (!session.needs_save_prompt() || !session.time_advanced_since_save()) {
            std::cerr << before << " -> " << session.date_text() << "\n";
            return fail(5, "months of play are not unsaved work (C-1)");
        }
        if (!session.save_city_as(city) || session.needs_save_prompt()) {
            return fail(6, "saving again did not clear the save prompt");
        }
        if (!session.load_city(city) || session.needs_save_prompt()) {
            return fail(7, "a freshly loaded city asks to be saved");
        }
        // Paused: no time passes, so no prompt.
        session.set_speed(0);
        if (!session.save_city_as(city)) {
            return fail(8, "could not save the paused city");
        }
        for (int i = 0; i < 2000; ++i) {
            session.tick();
        }
        if (session.needs_save_prompt()) {
            return fail(9, "a paused city became unsaved with no change");
        }
    }

    // C-5's historyInitialized fix is a default member initialiser; the
    // UBSan build of this suite is what checks it (no invalid bool loads).

    // C-6 and C-5: corrupt cityTime, game level, and city class in a file.
    const std::string corrupt = std::string(dir) + "/corrupt.cty";
    {
        Micropolis sim;
        sim.generateSomeCity(11);
        sim.cityTime = 0x00ABCDE1; // distinctive and far past year 9999
        if (!sim.saveFile(corrupt.c_str())) {
            return fail(11, "could not write the corrupt fixture");
        }
    }
    std::vector<unsigned char> data = read_all(corrupt);
    std::size_t time_at = std::string::npos;
    for (std::size_t i = 0; i + 3 < data.size() && i < 27120; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0xAB && data[i + 2] == 0xCD && data[i + 3] == 0xE1) {
            time_at = i;
            break;
        }
    }
    if (time_at == std::string::npos || time_at < 16) {
        return fail(12, "could not find cityTime in the saved file");
    }
    const std::size_t misc = time_at - 16; // cityTime is miscHist[8..9]
    put_be16(data, misc + 30, 0x7fff);     // miscHist[15] game level
    put_be16(data, misc + 32, 0x7fff);     // miscHist[16] city class
    write_all(corrupt, data);
    // Round 13 (C-1) supersedes the C-6 clamp: a date far past year 9999 is
    // garbage, so the file is refused with a reason and the city untouched.
    {
        Micropolis sim;
        if (sim.loadFile(corrupt.c_str()) || sim.loadErrorDetail.find("date") == std::string::npos) {
            return fail(13, "the impossible-date fixture loaded or gave no reason (r13 C-1)");
        }
        CitySession session;
        if (session.load_city(corrupt) || session.last_load_error().find("date") == std::string::npos) {
            std::cerr << "error " << session.last_load_error() << "\n";
            return fail(17, "the session accepted the impossible-date fixture (r13 C-1)");
        }
    }
    // C-5: with a plausible date, an out-of-range level and class are clamped.
    put_be16(data, time_at, 0x0000);
    put_be16(data, time_at + 2, 0x0100);
    write_all(corrupt, data);
    {
        Micropolis sim;
        if (!sim.loadFile(corrupt.c_str())) {
            return fail(13, "the corrupt level/class fixture did not load");
        }
        const long long max_time = static_cast<long long>(9999 - 1900) * 48;
        if (sim.cityTime < 0 || sim.cityTime > max_time) {
            std::cerr << "cityTime " << static_cast<long long>(sim.cityTime) << "\n";
            return fail(14, "a corrupted save set an impossible date (C-6)");
        }
        if (sim.gameLevel < LEVEL_FIRST || sim.gameLevel > LEVEL_LAST) {
            return fail(15, "an out-of-range game level was kept (C-5)");
        }
        if (sim.cityClass < CC_VILLAGE || sim.cityClass > CC_MEGALOPOLIS) {
            return fail(16, "an out-of-range city class was kept (C-5)");
        }
    }

    std::remove(city.c_str());
    std::remove(corrupt.c_str());
    rmdir(dir);
    std::cout << "round8 ok\n";
    return 0;
}

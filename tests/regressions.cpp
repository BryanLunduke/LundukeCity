// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless checks for the 0.8-2 fixes: query wording, save/load, budget
// timing, evaluation, disasters' view target, and sprite cleanup.

#include "city_session.hpp"
#include "messages.hpp"

#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

int count_sprites(const Micropolis &sim)
{
    int count = 0;
    for (SimSprite *sprite = sim.spriteList; sprite != nullptr; sprite = sprite->next) {
        ++count;
        if (count > 100000) {
            return count;
        }
    }
    return count;
}

bool near(float value, float expected)
{
    const float gap = value - expected;
    return gap > -0.02f && gap < 0.02f;
}

} // namespace

static int test_query_words()
{
    const std::string low = zone_status_text(29, 1, 5, 9, 13, 17);
    const std::string high = zone_status_text(11, 4, 8, 12, 16, 20);
    if (low.find("sparse") == std::string::npos || low.find("low") == std::string::npos ||
        low.find("safe") == std::string::npos || low.find("clean") == std::string::npos ||
        low.find("declining") == std::string::npos || low.find("booming") != std::string::npos) {
        std::fprintf(stderr, "low query: %s\n", low.c_str());
        return fail(1, "low query bands did not follow the engine categories");
    }
    if (high.find("packed") == std::string::npos || high.find("prime") == std::string::npos ||
        high.find("dangerous") == std::string::npos || high.find("heavy") == std::string::npos ||
        high.find("booming") == std::string::npos || high.find("sparse") != std::string::npos) {
        std::fprintf(stderr, "high query: %s\n", high.c_str());
        return fail(2, "high query bands did not follow the engine categories");
    }

    CitySession session;
    session.new_city("Query", 5);
    bool found = false;
    for (int y = 0; y < CitySession::kWorldH && !found; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            if ((session.map_value(x, y) & LOMASK) != DIRT) {
                continue;
            }
            session.use_tool(TOOL_QUERY, x, y);
            found = true;
            break;
        }
    }
    if (!found) {
        return fail(3, "generated city had no clear tile to query");
    }
    const std::string live = session.message();
    if (live.find("growth stable") == std::string::npos || live.find("booming") != std::string::npos) {
        std::fprintf(stderr, "live query: %s\n", live.c_str());
        return fail(4, "clear land was not reported as stable growth");
    }
    return 0;
}

static int test_save_round_trip()
{
    const std::string path = "/tmp/lunduke-harbor-town.cty";
    const std::string stem_path = "/tmp/Old Stem.cty";
    Micropolis sim;
    sim.generateSomeCity(9);
    sim.setCleanCityName("Harbor Town");
    sim.roadPercent = 0.4f;
    sim.policePercent = 0.4f;
    sim.firePercent = 0.4f;
    sim.crimeRamp = 80;
    sim.pollutionRamp = 90;
    sim.miscHist[10] = 80;
    sim.miscHist[11] = 90;
    sim.crimeHist[0] = 80;
    sim.crimeHist[4] = 0x1234;
    sim.pollutionHist[0] = 90;
    sim.pollutionHist[4] = 0x2345;
    sim.cityTime = 1234;
    sim.map[3][3] = 0x1234;

    const short scrambled_guard = sim.map[3][3];
    if (sim.saveFile("/dev/full")) {
        return fail(5, "saving to a full disk should fail");
    }
    if (sim.map[3][3] != scrambled_guard || sim.crimeHist[4] != 0x1234) {
        return fail(6, "a failed save left the live city byte-swapped");
    }

    sim.saveCityAs(path.c_str());
    if (sim.cityName != "Harbor Town") {
        return fail(7, "save renamed the city from the filename");
    }

    Micropolis loaded;
    if (!loaded.loadCity(path.c_str()) || loaded.cityName != "Harbor Town") {
        std::fprintf(stderr, "loaded name '%s'\n", loaded.cityName.c_str());
        return fail(8, "stored city name did not survive load");
    }
    if (!near(loaded.roadPercent, 0.4f) || !near(loaded.policePercent, 0.4f) ||
        !near(loaded.firePercent, 0.4f)) {
        std::fprintf(stderr, "funding %f %f %f\n", loaded.roadPercent, loaded.policePercent,
                     loaded.firePercent);
        return fail(9, "loaded funding was not the saved 40 percent");
    }
    if (loaded.roadEffect >= MAX_ROAD_EFFECT || loaded.policeEffect >= MAX_POLICE_STATION_EFFECT ||
        loaded.fireEffect >= MAX_FIRE_STATION_EFFECT) {
        return fail(10, "service effect after load was still full funding");
    }
    if (loaded.cityTime != 1234 || loaded.crimeRamp != 80 || loaded.pollutionRamp != 90 ||
        loaded.crimeHist[4] != 0x1234 || loaded.pollutionHist[4] != 0x2345 ||
        loaded.map[3][3] != 0x1234) {
        std::fprintf(stderr, "time %ld ramp %d %d crime %d poll %d map %d\n",
                     static_cast<long>(loaded.cityTime), loaded.crimeRamp, loaded.pollutionRamp,
                     loaded.crimeHist[4], loaded.pollutionHist[4], loaded.map[3][3]);
        return fail(11, "history, ramps, or city time did not round-trip");
    }

    loaded.taxFund = 2000;
    loaded.roadFund = 1000;
    loaded.policeFund = 0;
    loaded.fireFund = 0;
    loaded.autoBudget = true;
    loaded.totalFunds = 5000;
    loaded.doBudgetNow(false);
    const long billed = static_cast<long>(loaded.totalFunds) - 5000;
    // 40% of a 1000 road request leaves about 1600 of the 2000 tax.
    // 100% would leave 1000.
    if (billed < 1400 || billed > 1700) {
        std::fprintf(stderr, "tax-year delta %ld\n", billed);
        return fail(12, "the tax year after load did not bill the saved percent");
    }

    std::ifstream in(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 27120) {
        return fail(13, "save was shorter than a classic city file");
    }
    std::ofstream out(stem_path, std::ios::binary);
    out.write(bytes.data(), 27120);
    out.close();
    Micropolis old_file;
    if (!old_file.loadCity(stem_path.c_str()) || old_file.cityName != "Old Stem") {
        std::fprintf(stderr, "old-file name '%s'\n", old_file.cityName.c_str());
        return fail(14, "a save with no name field should fall back to the filename stem");
    }

    loaded.setCleanCityName("Port Side");
    const std::string renamed = "/tmp/not-the-name.cty";
    loaded.saveCityAs(renamed.c_str());
    Micropolis again;
    if (!again.loadCity(renamed.c_str()) || again.cityName != "Port Side") {
        std::fprintf(stderr, "renamed '%s'\n", again.cityName.c_str());
        return fail(15, "rename did not survive the next save and load");
    }

    std::remove(path.c_str());
    std::remove(stem_path.c_str());
    std::remove(renamed.c_str());
    return 0;
}

static int test_budget_timing()
{
    Micropolis sim;
    sim.generateSomeCity(3);
    sim.autoBudget = false;
    sim.taxFund = 1000;
    sim.roadFund = 500;
    sim.policeFund = 500;
    sim.fireFund = 500;
    sim.roadPercent = 1.0f;
    sim.policePercent = 1.0f;
    sim.firePercent = 1.0f;
    sim.setFunds(10000);
    const long before = static_cast<long>(sim.totalFunds);
    sim.doBudgetNow(false);
    if (static_cast<long>(sim.totalFunds) != before || !sim.budgetAwaitingAccept) {
        return fail(16, "manual budget charged before the window could change the rates");
    }
    if (!sim.budgetAnchorValid || sim.budgetAnchorFunds != before) {
        return fail(17, "previous funds were not the balance before this budget");
    }
    sim.roadPercent = 0.4f;
    sim.policePercent = 0.4f;
    sim.firePercent = 0.4f;
    sim.commitBudgetPayment();
    const long charged = static_cast<long>(sim.totalFunds);
    // Taxes 1000, spend 40% of 1500 = 600, cash flow +400.
    if (charged != before + 400) {
        std::fprintf(stderr, "funds %ld\n", charged);
        return fail(18, "accepting the budget did not bill the rates from the window");
    }
    sim.commitBudgetPayment();
    if (static_cast<long>(sim.totalFunds) != charged) {
        return fail(19, "closing the budget window charged twice");
    }

    sim.autoBudget = true;
    sim.budgetAwaitingAccept = false;
    sim.taxFund = 1000;
    sim.roadFund = 100;
    sim.policeFund = 100;
    sim.fireFund = 100;
    sim.roadPercent = 1.0f;
    sim.policePercent = 1.0f;
    sim.firePercent = 1.0f;
    sim.setFunds(4000);
    const long auto_before = static_cast<long>(sim.totalFunds);
    sim.doBudgetNow(false);
    if (sim.budgetAwaitingAccept || static_cast<long>(sim.totalFunds) == auto_before) {
        return fail(20, "auto budget waited on the window");
    }
    return 0;
}

static int test_evaluation_delta()
{
    Micropolis sim;
    sim.resPop = 100;
    sim.comPop = 0;
    sim.indPop = 0;
    sim.totalPop = 10;
    sim.cityPop = -1;
    sim.cityPopDelta = 0;
    sim.cityEvaluation();
    const Quad baseline_pop = sim.cityPop;
    const Quad baseline_delta = sim.cityPopDelta;
    const short baseline_score = sim.cityScore;
    if (baseline_pop != 2000 || baseline_delta != 0) {
        std::fprintf(stderr, "baseline pop %ld delta %ld\n", static_cast<long>(baseline_pop),
                     static_cast<long>(baseline_delta));
        return fail(21, "the first evaluation did not record a zero migration");
    }

    sim.resPop = 300;
    sim.cityEvaluationPreview();
    if (sim.cityPop != baseline_pop || sim.cityPopDelta != baseline_delta ||
        sim.cityScore != baseline_score) {
        return fail(22, "opening evaluation consumed the yearly migration or score");
    }

    sim.cityEvaluation();
    if (sim.cityPop != 6000 || sim.cityPopDelta != 4000) {
        std::fprintf(stderr, "january pop %ld delta %ld\n", static_cast<long>(sim.cityPop),
                     static_cast<long>(sim.cityPopDelta));
        return fail(23, "January did not report the year's migration");
    }
    return 0;
}

static int test_view_and_sprites()
{
    CitySession session;
    session.new_city("Goto", 4);
    session.set_auto_goto(true);
    session.disaster_earthquake();
    int x = -1;
    int y = -1;
    if (!session.take_view_target(x, y) || !Micropolis::testBounds(x, y)) {
        return fail(24, "earthquake did not move the view");
    }
    if (session.take_earthquake() <= 0) {
        return fail(25, "earthquake did not report a shake");
    }

    session.set_auto_goto(false);
    session.disaster_monster();
    if (session.take_view_target(x, y)) {
        return fail(26, "auto-goto off still moved the view");
    }

    Micropolis sim;
    for (int i = 0; i < 80; ++i) {
        sim.newSprite("", SPRITE_EXPLOSION, i, i);
    }
    if (count_sprites(sim) < 80) {
        return fail(27, "explosions were not created");
    }
    sim.destroyAllSprites();
    if (sim.spriteList != nullptr || count_sprites(sim) != 0) {
        return fail(28, "destroyAllSprites left deactivated sprites on the list");
    }
    sim.makeSprite(SPRITE_TRAIN, 16, 16);
    sim.makeSprite(SPRITE_AIRPLANE, 32, 32);
    sim.makeSprite(SPRITE_SHIP, 48, 48);
    sim.makeSprite(SPRITE_MONSTER, 64, 64);
    if (sim.getSprite(SPRITE_TRAIN) == nullptr || sim.getSprite(SPRITE_AIRPLANE) == nullptr ||
        sim.getSprite(SPRITE_SHIP) == nullptr || sim.getSprite(SPRITE_MONSTER) == nullptr) {
        return fail(29, "a playing city lost its trains, planes, ships, or monster");
    }

    CitySession cities;
    cities.new_city("First", 6);
    cities.place_sprite(SPRITE_TRAIN, 10, 10);
    cities.place_sprite(SPRITE_AIRPLANE, 12, 12);
    cities.place_sprite(SPRITE_SHIP, 14, 14);
    cities.place_sprite(SPRITE_MONSTER, 16, 16);
    int seen = 0;
    for (const auto &dot : cities.sprites()) {
        if (dot.frame > 0 && (dot.type == SPRITE_TRAIN || dot.type == SPRITE_AIRPLANE ||
                              dot.type == SPRITE_SHIP || dot.type == SPRITE_MONSTER)) {
            ++seen;
        }
    }
    if (seen != 4) {
        return fail(30, "sprites() did not report the vehicles in a playing city");
    }
    if (!cities.auto_goto()) {
        return fail(32, "a new city should start with auto-goto on");
    }
    cities.set_auto_goto(false);
    const std::string goto_path = "/tmp/lunduke-auto-goto.cty";
    if (!cities.save_city_as(goto_path)) {
        return fail(33, "could not save the auto-goto flag");
    }
    CitySession loaded_goto;
    if (loaded_goto.load_city("/tmp/does-not-exist-lunduke.cty") ||
        loaded_goto.message().find("Could not load") == std::string::npos) {
        return fail(34, "a missing city file did not report an error");
    }
    if (!loaded_goto.load_city(goto_path) || loaded_goto.auto_goto() ||
        loaded_goto.message() != "Loaded a saved city.") {
        std::fprintf(stderr, "goto %d message '%s'\n", loaded_goto.auto_goto() ? 1 : 0,
                     loaded_goto.message().c_str());
        return fail(35, "load did not keep auto-goto off or replace the error");
    }
    cities.set_auto_goto(true);
    if (!cities.save_city_as(goto_path) || !loaded_goto.load_city(goto_path) || !loaded_goto.auto_goto() ||
        loaded_goto.message() != "Loaded a saved city.") {
        return fail(36, "load did not honor a saved auto-goto flag");
    }
    std::remove(goto_path.c_str());

    CitySession noisy;
    noisy.new_city("Siren", 4);
    noisy.set_sound_enabled(true);
    noisy.set_auto_goto(true);
    noisy.disaster_earthquake();
    bool siren = false;
    for (const auto &sound : noisy.take_sounds()) {
        if (sound == "Siren") {
            siren = true;
        }
    }
    if (!siren) {
        return fail(37, "earthquake message did not queue a siren");
    }
    int view_x = 0;
    int view_y = 0;
    if (!noisy.take_view_target(view_x, view_y)) {
        return fail(38, "auto-goto on did not move the earthquake view");
    }

    cities.new_city("Second", 7);
    for (const auto &dot : cities.sprites()) {
        if (dot.type == SPRITE_TRAIN || dot.type == SPRITE_AIRPLANE || dot.type == SPRITE_SHIP ||
            dot.type == SPRITE_MONSTER) {
            return fail(31, "new city kept sprites from the previous city");
        }
    }
    return 0;
}

int main()
{
    if (const int code = test_query_words()) {
        return code;
    }
    if (const int code = test_save_round_trip()) {
        return code;
    }
    if (const int code = test_budget_timing()) {
        return code;
    }
    if (const int code = test_evaluation_delta()) {
        return code;
    }
    if (const int code = test_view_and_sprites()) {
        return code;
    }
    return 0;
}

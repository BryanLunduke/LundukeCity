// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless checks for the 0.8-2 fixes: query wording, save/load, budget
// timing, evaluation, disasters' view target, and sprite cleanup.

#include "city_seed.hpp"
#include "city_session.hpp"
#include "messages.hpp"
#include "save_path.hpp"
#include "sound_player.hpp"

#include "micropolis.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
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

static std::vector<char> read_bytes(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static int count_wood(const Micropolis &sim)
{
    int woods = 0;
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            const int tile = sim.map[x][y] & LOMASK;
            if (tile >= WOODS_LOW && tile <= WOODS5) {
                ++woods;
            }
        }
    }
    return woods;
}

static void write_pcm_wav(const std::string &path, int frames)
{
    const int data_bytes = frames * 2;
    const int riff = 36 + data_bytes;
    std::vector<unsigned char> bytes(static_cast<std::size_t>(44 + data_bytes), 0);
    std::memcpy(bytes.data(), "RIFF", 4);
    bytes[4] = static_cast<unsigned char>(riff & 0xff);
    bytes[5] = static_cast<unsigned char>((riff >> 8) & 0xff);
    bytes[6] = static_cast<unsigned char>((riff >> 16) & 0xff);
    bytes[7] = static_cast<unsigned char>((riff >> 24) & 0xff);
    std::memcpy(bytes.data() + 8, "WAVE", 4);
    std::memcpy(bytes.data() + 12, "fmt ", 4);
    bytes[16] = 16;
    bytes[20] = 1;
    bytes[22] = 1;
    bytes[24] = 22050 & 0xff;
    bytes[25] = (22050 >> 8) & 0xff;
    bytes[34] = 16;
    std::memcpy(bytes.data() + 36, "data", 4);
    bytes[40] = static_cast<unsigned char>(data_bytes & 0xff);
    bytes[41] = static_cast<unsigned char>((data_bytes >> 8) & 0xff);
    bytes[42] = static_cast<unsigned char>((data_bytes >> 16) & 0xff);
    bytes[43] = static_cast<unsigned char>((data_bytes >> 24) & 0xff);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

static int test_hostile_review()
{
    CitySession census;
    census.new_city("Census", 11);
    if (hostile_review_session_probe(census, 2) != 0) {
        return fail(50, "could not stage a finished census");
    }
    const long settled = census.evaluation().population;
    if (settled <= 0) {
        std::fprintf(stderr, "settled population %ld\n", settled);
        return fail(51, "a finished census did not report the neighborhood");
    }
    census.tick();
    if (census.census_ready() || census.evaluation().population != settled) {
        std::fprintf(stderr, "ready %d pop %ld settled %ld\n", census.census_ready() ? 1 : 0,
                     census.evaluation().population, settled);
        return fail(52, "population dropped when phase 0 cleared the census");
    }
    if (hostile_review_session_probe(census, 0) != 0) {
        return fail(53, "note_month at a month boundary stored the cleared census");
    }

    Micropolis preview;
    preview.resPop = 200;
    preview.comPop = 10;
    preview.indPop = 10;
    preview.totalPop = 40;
    preview.cityPop = 1000;
    preview.cityPopDelta = 10;
    preview.cityScore = 500;
    preview.cityAssessedValue = 80;
    const UQuad random_before = preview.randomState();
    preview.cityEvaluationPreview();
    if (preview.randomState() != random_before || preview.cityPop != 1000 || preview.cityScore != 500 ||
        preview.cityAssessedValue != 80 || !preview.evalPreviewValid) {
        return fail(54, "evaluation preview advanced the RNG or kept fields it writes");
    }

    Micropolis tax_year;
    tax_year.generateSomeCity(8);
    tax_year.autoBudget = false;
    tax_year.taxFund = 2000;
    tax_year.roadFund = 500;
    tax_year.policeFund = 0;
    tax_year.fireFund = 0;
    tax_year.roadPercent = 1.0f;
    tax_year.policePercent = 0.0f;
    tax_year.firePercent = 0.0f;
    tax_year.setFunds(5000);
    tax_year.doBudgetNow(false);
    if (!tax_year.budgetAwaitingAccept || static_cast<long>(tax_year.totalFunds) != 5000) {
        return fail(55, "the tax year was not waiting on the budget window");
    }
    const std::string tax_path = "/tmp/lunduke-tax-year.cty";
    tax_year.saveCityAs(tax_path.c_str());
    Micropolis tax_loaded;
    if (tax_year.budgetAwaitingAccept || !tax_loaded.loadCity(tax_path.c_str()) ||
        static_cast<long>(tax_loaded.totalFunds) != 6500) {
        std::fprintf(stderr, "loaded funds %ld awaiting %d\n", static_cast<long>(tax_loaded.totalFunds),
                     tax_year.budgetAwaitingAccept ? 1 : 0);
        return fail(56, "saving a waiting tax year dropped that year's cash flow");
    }
    std::remove(tax_path.c_str());

    CitySession funded;
    funded.new_city("Rates", 3);
    funded.set_road_funding(0);
    funded.set_police_funding(0);
    funded.set_fire_funding(0);
    hostile_review_session_probe(funded, 1);
    funded.new_city("Rates Two", 4);
    const CitySession::BudgetBook reset = funded.budget();
    if (reset.road_percent != 100 || reset.police_percent != 100 || reset.fire_percent != 100 ||
        funded.budget_pending()) {
        std::fprintf(stderr, "funding %d %d %d pending %d\n", reset.road_percent, reset.police_percent,
                     reset.fire_percent, funded.budget_pending() ? 1 : 0);
        return fail(57, "new city kept the previous funding rates");
    }

    Micropolis broke;
    broke.generateSomeCity(2);
    broke.autoBudget = false;
    broke.taxFund = 1000;
    broke.roadFund = 5000;
    broke.policeFund = 0;
    broke.fireFund = 0;
    broke.roadPercent = 1.0f;
    broke.policePercent = 1.0f;
    broke.firePercent = 1.0f;
    broke.setFunds(100);
    broke.doBudgetNow(false);
    broke.roadPercent = 1.0f;
    broke.policePercent = 1.0f;
    broke.firePercent = 1.0f;
    broke.commitBudgetPayment();
    if (broke.totalFunds < 0) {
        std::fprintf(stderr, "funds %ld\n", static_cast<long>(broke.totalFunds));
        return fail(58, "accepting the budget spent cash the city does not have");
    }

    int island_seed = -1;
    for (int s = 0; s < 4000 && island_seed < 0; ++s) {
        Micropolis probe;
        probe.primeRandom(s);
        if (probe.rollRandom(100) < 10) {
            island_seed = s;
        }
    }
    if (island_seed < 0) {
        return fail(59, "no seed rolled an island");
    }
    Micropolis island;
    island.terrainCreateIsland = -1;
    island.terrainCurveLevel = 0;
    island.terrainLakeLevel = 0;
    island.terrainTreeLevel = 0;
    island.generateMap(island_seed);
    int edge_water = 0;
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            if (x >= 5 && x < WORLD_W - 5 && y >= 5 && y < WORLD_H - 5) {
                continue;
            }
            const int tile = island.map[x][y] & LOMASK;
            if ((tile >= RIVER && tile <= WATER_HIGH) || tile == REDGE) {
                ++edge_water;
            }
        }
    }
    if (count_wood(island) != 0 || edge_water < 100) {
        std::fprintf(stderr, "woods %d edge %d seed %d\n", count_wood(island), edge_water, island_seed);
        return fail(60, "the default island ignored no-trees or was not an island");
    }

    Micropolis kept;
    kept.generateSomeCity(6);
    kept.setCleanCityName("Keep Name");
    const std::string atomic = "/tmp/lunduke-atomic.cty";
    kept.saveCityAs(atomic.c_str());
    const std::vector<char> previous = read_bytes(atomic);
    if (previous.size() < 27120) {
        return fail(61, "atomic-save fixture was not a city file");
    }
    const std::string tmp_dir = atomic + ".tmp";
    if (mkdir(tmp_dir.c_str(), 0755) != 0) {
        return fail(62, "could not block the save temporary file");
    }
    kept.cityFileName = "keep-me";
    const short map_guard = kept.map[1][1];
    if (kept.saveCityAs(atomic.c_str()) || kept.cityFileName != "keep-me" || kept.map[1][1] != map_guard ||
        read_bytes(atomic) != previous) {
        rmdir(tmp_dir.c_str());
        return fail(63, "a failed save replaced the previous city file or the engine path");
    }
    rmdir(tmp_dir.c_str());
    const std::string link = "/tmp/lunduke-atomic-link.cty";
    std::remove(link.c_str());
    if (symlink(atomic.c_str(), link.c_str()) != 0 || kept.saveFile(link.c_str()) || read_bytes(atomic) != previous) {
        std::remove(link.c_str());
        return fail(64, "save followed a symlink and changed the target");
    }
    std::remove(link.c_str());
    std::remove(atomic.c_str());

    if (with_cty_suffix("Town") != "Town.cty" || with_cty_suffix("Town.CTY") != "Town.CTY" ||
        with_cty_suffix("notes.cty.bak") != "notes.cty.bak.cty" ||
        with_cty_suffix("/tmp/Town.cty") != "/tmp/Town.cty") {
        return fail(65, "the save path was not the confirmed name plus a .cty suffix");
    }

    int seed = 99;
    bool seed_was_set = true;
    if (take_city_seed("12a", seed, seed_was_set) || !seed_was_set || seed != 99) {
        return fail(66, "a seed typo was treated as a clock seed");
    }
    if (!take_city_seed("12", seed, seed_was_set) || !seed_was_set || seed != 12) {
        return fail(67, "a whole-number seed was rejected");
    }

    Micropolis named;
    named.generateSomeCity(4);
    named.setCleanCityName("Harbor");
    named.resourceDir = "/tmp/lunduke-missing-scenarios";
    if (named.loadScenario(SC_HAMBURG) || named.cityName != "Harbor") {
        std::fprintf(stderr, "scenario name '%s'\n", named.cityName.c_str());
        return fail(68, "a failed scenario load did not keep the previous city");
    }
    CitySession playing;
    playing.new_city("Harbor", 4);
    if (playing.load_scenario(99) || playing.city_name() != "Harbor") {
        return fail(69, "load_scenario reported success or renamed the city");
    }

    Micropolis flow;
    flow.generateSomeCity(1);
    flow.totalPop = 20000;
    flow.landValueAverage = 120;
    flow.cityTax = 10;
    flow.gameLevel = LEVEL_EASY;
    flow.roadTotal = 0;
    flow.railTotal = 0;
    flow.policeStationPop = 0;
    flow.fireStationPop = 0;
    flow.taxFlag = false;
    flow.autoBudget = true;
    flow.setFunds(5000000);
    flow.collectTax();
    const Quad expected = flow.taxFund - (flow.policeFund + flow.fireFund + flow.roadFund);
    if (flow.cashFlow != expected || expected <= 32767) {
        std::fprintf(stderr, "cash %ld expected %ld\n", static_cast<long>(flow.cashFlow),
                     static_cast<long>(expected));
        return fail(70, "cash flow was truncated to 16 bits");
    }

    Micropolis titled;
    titled.generateSomeCity(1);
    const std::string evil_path = "/tmp/lunduke-evil-name.cty";
    titled.setCleanCityName("Plain");
    titled.saveCityAs(evil_path.c_str());
    std::vector<char> classic = read_bytes(evil_path);
    if (classic.size() < 27120) {
        return fail(71, "could not build a city file for the name filter");
    }
    classic.resize(27120);
    const std::string evil = std::string("Line\nOne") + "\xE2\x80\xAE";
    classic.push_back('L');
    classic.push_back('C');
    classic.push_back('N');
    classic.push_back('1');
    classic.push_back(static_cast<char>(evil.size() & 0xff));
    classic.push_back(static_cast<char>((evil.size() >> 8) & 0xff));
    classic.insert(classic.end(), evil.begin(), evil.end());
    {
        std::ofstream out(evil_path, std::ios::binary);
        out.write(classic.data(), static_cast<std::streamsize>(classic.size()));
    }
    Micropolis filtered;
    if (!filtered.loadCity(evil_path.c_str()) || filtered.cityName != "LineOne" ||
        filtered.cityName.find('\n') != std::string::npos) {
        std::fprintf(stderr, "filtered name '%s'\n", filtered.cityName.c_str());
        return fail(72, "a loaded city name skipped the rename filter");
    }
    std::remove(evil_path.c_str());

    CitySession dirty;
    dirty.new_city("Dirty", 5);
    if (dirty.needs_save_prompt()) {
        return fail(73, "a fresh city asked to be saved");
    }
    bool laid = false;
    for (int y = 2; y < CitySession::kWorldH - 2 && !laid; ++y) {
        for (int x = 2; x < CitySession::kWorldW - 2; ++x) {
            if ((dirty.map_value(x, y) & LOMASK) == DIRT) {
                dirty.use_tool(TOOL_ROAD, x, y);
                laid = true;
                break;
            }
        }
    }
    if (!laid || !dirty.needs_save_prompt()) {
        return fail(74, "laying a road did not mark the city unsaved");
    }
    const std::string dirty_path = "/tmp/lunduke-dirty.cty";
    if (!dirty.save_city_as(dirty_path) || dirty.needs_save_prompt()) {
        return fail(75, "a successful save left the city dirty");
    }
    dirty.new_city("Clean", 6);
    if (dirty.needs_save_prompt()) {
        return fail(76, "generating a city kept the previous dirty bit");
    }
    std::remove(dirty_path.c_str());

    const std::string sound_dir = "/tmp/lunduke-sounds";
    mkdir(sound_dir.c_str(), 0755);
    const std::string small_wav = sound_dir + "/siren.wav";
    const std::string large_wav = sound_dir + "/Siren.wav";
    const std::string huge_wav = sound_dir + "/huge.wav";
    const std::string linked = sound_dir + "/linked.wav";
    write_pcm_wav(small_wav, 8);
    write_pcm_wav(large_wav, 40);
    {
        std::ofstream huge(huge_wav, std::ios::binary);
        std::vector<char> chunk(1024 * 1024, '\0');
        huge.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        huge.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
        huge.write(chunk.data(), 64);
    }
    std::remove(linked.c_str());
    symlink(small_wav.c_str(), linked.c_str());
    if (preview_wav_file(huge_wav) || preview_wav_file(linked) || !preview_wav_file(small_wav)) {
        return fail(77, "wav decode did not reject an oversized file or a symlink");
    }
    const auto indexed = index_sound_directory(sound_dir);
    std::string first;
    std::error_code index_ec;
    for (const auto &entry : std::filesystem::directory_iterator(sound_dir, index_ec)) {
        if (index_ec || entry.symlink_status().type() != std::filesystem::file_type::regular) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.size() < 5 || name.substr(name.size() - 4) != ".wav") {
            continue;
        }
        if (entry.file_size() > kMaxWavBytes) {
            continue;
        }
        std::string key;
        for (unsigned char ch : name.substr(0, name.size() - 4)) {
            if (ch == '.' || ch == '-' || ch == '_' || ch == ' ') {
                continue;
            }
            if (ch >= 'A' && ch <= 'Z') {
                ch = static_cast<unsigned char>(ch - 'A' + 'a');
            }
            key.push_back(static_cast<char>(ch));
        }
        if (key == "siren") {
            first = entry.path().string();
            break;
        }
    }
    const auto found = indexed.find("siren");
    if (first.empty() || found == indexed.end() || found->second != first || indexed.count("linked") != 0 ||
        indexed.count("huge") != 0 || kMaxQueuedSounds > 32) {
        std::fprintf(stderr, "first '%s'\n", first.c_str());
        return fail(78, "the sound index followed a symlink or kept the larger clip");
    }
    std::remove(small_wav.c_str());
    std::remove(large_wav.c_str());
    std::remove(huge_wav.c_str());
    std::remove(linked.c_str());
    rmdir(sound_dir.c_str());

    // Speed 1 steps the simulator, and therefore sprites, on every 5th pass.
    // Speed 3 steps on every pass. generateSomeCity() finishes in
    // initWillStuff(), which reseeds the RNG from the clock. A live tornado
    // then dies when getRandom(500) == 0, and a disaster can call
    // makeTornado() and put the lifetime counter back to 200. Either one
    // freezes the step count after a few moves (the flake was
    // "sprite steps slow 6 fast 3"). Count the moves themselves: a tornado
    // with a spent lifetime still walks, and that blow-away roll is skipped.
    auto sprite_steps = [](int speed) {
        Micropolis sim;
        sim.generateSomeCity(2);
        sim.setEnableDisasters(false);
        sim.primeRandom(1);
        sim.setSpeed(static_cast<short>(speed));
        sim.setPasses(1);
        sim.speedCycle = 0;
        sim.makeTornado();
        SimSprite *sprite = sim.getSprite(SPRITE_TORNADO);
        if (sprite == nullptr) {
            return -1;
        }
        const int home_x = (WORLD_W << 4) / 2;
        const int home_y = (WORLD_H << 4) / 2;
        sprite->count = 0;
        sprite->frame = 1;
        int steps = 0;
        for (int i = 0; i < 30; ++i) {
            sprite->x = home_x;
            sprite->y = home_y;
            sim.simTick();
            if (sprite->frame == 0 || sim.getSprite(SPRITE_TORNADO) != sprite) {
                return -1;
            }
            if (sprite->x != home_x || sprite->y != home_y) {
                ++steps;
            }
        }
        return steps;
    };
    const int slow_steps = sprite_steps(1);
    const int fast_steps = sprite_steps(3);
    // 30 ticks, one pass each: cycles 5, 10, ..., 30 move at Slow (6), and
    // every tick moves at Fast (30).
    if (slow_steps != 6 || fast_steps != 30) {
        std::fprintf(stderr, "sprite steps slow %d fast %d\n", slow_steps, fast_steps);
        return fail(79, "slow speed did not slow sprites with the simulator");
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
    if (const int code = test_hostile_review()) {
        return code;
    }
    return 0;
}

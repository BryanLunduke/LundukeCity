// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless regressions for the round-4 review (findings 1-3 and 5-7).
// The budget window's buttons are covered by ui_round4.

#include "city_session.hpp"
#include "tools.hpp"

#include "micropolis.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

short file_slot(const std::string &path, int index)
{
    std::ifstream in(path, std::ios::binary);
    const std::size_t offset = 6u * 240u * 2u + static_cast<std::size_t>(index) * 2u;
    in.seekg(static_cast<std::streamoff>(offset));
    unsigned char bytes[2] = {0, 0};
    in.read(reinterpret_cast<char *>(bytes), 2);
    if (!in) {
        return -1;
    }
    return static_cast<short>((bytes[0] << 8) | bytes[1]);
}

const ToolDef *tool_named(int engine_id)
{
    for (int i = 0; i < kToolCount; ++i) {
        if (tool_by_index(i)->engine_id == engine_id) {
            return tool_by_index(i);
        }
    }
    return nullptr;
}

CitySession::NewCitySpec spec_for(const char *name, int seed, int level)
{
    CitySession::NewCitySpec spec;
    spec.name = name;
    spec.seed = seed;
    spec.seed_was_set = true;
    spec.difficulty = level;
    return spec;
}

bool dollars_match(const CitySession::BudgetBook &book, long treasury)
{
    const long spent = book.road_spent + book.police_spent + book.fire_spent;
    return book.taxes - spent == book.cash_flow && book.previous_funds == treasury &&
           book.funds == treasury + book.cash_flow && book.funds >= 0;
}

} // namespace

int main()
{
    const std::string path = "/tmp/lunduke-round4-level.cty";
    std::remove(path.c_str());

    CitySession paused;
    paused.set_speed(0);
    paused.new_city(spec_for("Hardville", 4, CitySession::kLevelHard));
    if (paused.funds() != 5000 || paused.difficulty() != CitySession::kLevelHard || paused.speed() != 0 ||
        !paused.save_city_as(path) || file_slot(path, 15) != CitySession::kLevelHard ||
        file_slot(path, 64) != 1) {
        std::fprintf(stderr, "hard funds %ld level %d slot %d disasters %d\n", paused.funds(), paused.difficulty(),
                     file_slot(path, 15), file_slot(path, 64));
        return fail(1, "a paused Hard city was not saved as Hard");
    }
    CitySession loaded_hard;
    loaded_hard.set_speed(3);
    if (!loaded_hard.load_city(path) || loaded_hard.difficulty() != CitySession::kLevelHard ||
        loaded_hard.funds() != 5000 || loaded_hard.speed() != 0 ||
        loaded_hard.evaluation().difficulty != "Hard" || !loaded_hard.disasters()) {
        std::fprintf(stderr, "reloaded level %d funds %ld speed %d diff '%s'\n", loaded_hard.difficulty(),
                     loaded_hard.funds(), loaded_hard.speed(), loaded_hard.evaluation().difficulty.c_str());
        return fail(2, "reloading the paused Hard city did not keep Hard, $5,000, and pause");
    }

    loaded_hard.new_city(spec_for("Easyville", 5, CitySession::kLevelEasy));
    if (loaded_hard.speed() != 0 || loaded_hard.funds() != 20000 ||
        loaded_hard.difficulty() != CitySession::kLevelEasy || !loaded_hard.save_city_as(path) ||
        file_slot(path, 15) != CitySession::kLevelEasy) {
        std::fprintf(stderr, "easy slot %d level %d funds %ld\n", file_slot(path, 15), loaded_hard.difficulty(),
                     loaded_hard.funds());
        return fail(3, "a paused Easy city after a Hard one was saved as the old level");
    }
    CitySession loaded_easy;
    if (!loaded_easy.load_city(path) || loaded_easy.difficulty() != CitySession::kLevelEasy ||
        loaded_easy.funds() != 20000 || loaded_easy.evaluation().difficulty != "Easy") {
        return fail(4, "reloading the paused Easy city came back as another level");
    }

    CitySession medium;
    medium.set_speed(0);
    medium.new_city(spec_for("Hard Again", 6, CitySession::kLevelHard));
    medium.new_city(spec_for("Midville", 7, CitySession::kLevelMedium));
    if (!medium.save_city_as(path) || file_slot(path, 15) != CitySession::kLevelMedium || medium.funds() != 10000) {
        std::fprintf(stderr, "medium slot %d funds %ld\n", file_slot(path, 15), medium.funds());
        return fail(5, "a paused Medium city was not saved as Medium");
    }
    CitySession loaded_medium;
    if (!loaded_medium.load_city(path) || loaded_medium.difficulty() != CitySession::kLevelMedium ||
        loaded_medium.funds() != 10000) {
        return fail(6, "reloading Medium did not keep Medium and $10,000");
    }

    Micropolis stale;
    stale.generateSomeCity(9);
    stale.gameLevel = LEVEL_HARD;
    stale.miscHist[15] = 0;
    stale.setFunds(5000);
    stale.setEnableDisasters(false);
    if (!stale.saveCityAs(path.c_str()) || file_slot(path, 15) != LEVEL_HARD || file_slot(path, 64) != 2) {
        std::fprintf(stderr, "stale slot %d disasters %d\n", file_slot(path, 15), file_slot(path, 64));
        return fail(7, "save left a stale difficulty slot, or moved the disasters flag");
    }
    Micropolis stale_loaded;
    if (!stale_loaded.loadCity(path.c_str()) || stale_loaded.gameLevel != LEVEL_HARD ||
        static_cast<long>(stale_loaded.totalFunds) != 5000 || stale_loaded.enableDisasters) {
        return fail(8, "a file written from a stale slot did not load as Hard with disasters off");
    }

    std::vector<char> classic;
    {
        std::ifstream in(path, std::ios::binary);
        classic.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    if (classic.size() < 27120) {
        return fail(9, "difficulty fixture was shorter than a classic city");
    }
    classic.resize(27120);
    const std::size_t level_off = 6u * 240u * 2u + 15u * 2u;
    const std::size_t disaster_off = 6u * 240u * 2u + 64u * 2u;
    classic[level_off] = 0;
    classic[level_off + 1] = static_cast<char>(LEVEL_HARD);
    classic[disaster_off] = 0;
    classic[disaster_off + 1] = 0;
    {
        std::ofstream out(path, std::ios::binary);
        out.write(classic.data(), static_cast<std::streamsize>(classic.size()));
    }
    Micropolis old_level;
    if (!old_level.loadCity(path.c_str()) || old_level.gameLevel != LEVEL_HARD || !old_level.enableDisasters ||
        old_level.miscHist[MISC_DISASTERS_SLOT] != 0) {
        return fail(10, "a classic file did not load its difficulty with disasters left on");
    }

    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        CitySession scenario;
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        if (!scenario.load_scenario(def.id) || scenario.city_name() != def.name || !scenario.disasters()) {
            std::fprintf(stderr, "scenario %s disasters %d\n", def.name, scenario.disasters() ? 1 : 0);
            return fail(11, "a bundled scenario no longer loads with disasters on");
        }
    }

    CitySession won;
    if (!won.load_scenario(SC_DULLSVILLE)) {
        return fail(12, "could not load Dullsville before a win");
    }
    won.set_speed(3);
    if (hostile_review_session_probe(won, 5) != 0 || !won.outcome_pause_pending() || won.outcome_resume_speed() != 3) {
        std::fprintf(stderr, "resume %d pending %d speed %d\n", won.outcome_resume_speed(),
                     won.outcome_pause_pending() ? 1 : 0, won.speed());
        return fail(13, "winning did not pause while remembering Fast");
    }
    if (!won.load_scenario(SC_SAN_FRANCISCO) || won.speed() != 3 || won.outcome_pause_pending() ||
        won.city_name() != "San Francisco") {
        std::fprintf(stderr, "next speed %d name '%s'\n", won.speed(), won.city_name().c_str());
        return fail(14, "the scenario after a win started paused instead of at Fast");
    }

    CitySession lost;
    if (!lost.load_scenario(SC_HAMBURG)) {
        return fail(15, "could not load Hamburg before a loss");
    }
    lost.set_speed(1);
    if (hostile_review_session_probe(lost, 6) != 0 || lost.outcome_resume_speed() != 1) {
        return fail(16, "losing did not remember Slow");
    }
    lost.resume_after_outcome();
    if (lost.speed() != 1 || lost.outcome_pause_pending()) {
        std::fprintf(stderr, "resumed %d pending %d\n", lost.speed(), lost.outcome_pause_pending() ? 1 : 0);
        return fail(17, "Keep playing did not restore Slow");
    }

    CitySession chosen;
    if (!chosen.load_scenario(SC_DULLSVILLE)) {
        return fail(18, "could not load Dullsville before choosing a speed");
    }
    chosen.set_speed(3);
    hostile_review_session_probe(chosen, 5);
    chosen.set_speed(2);
    if (chosen.outcome_pause_pending() || !chosen.load_scenario(SC_BERN) || chosen.speed() != 2) {
        std::fprintf(stderr, "chosen speed %d pending %d\n", chosen.speed(), chosen.outcome_pause_pending() ? 1 : 0);
        return fail(19, "a speed chosen after the win did not replace the remembered speed");
    }

    CitySession already;
    if (!already.load_scenario(SC_DULLSVILLE)) {
        return fail(20, "could not load Dullsville while already paused");
    }
    already.set_speed(0);
    hostile_review_session_probe(already, 5);
    if (already.outcome_resume_speed() != 0 || !already.load_scenario(SC_TOKYO) || already.speed() != 0) {
        std::fprintf(stderr, "already resume %d speed %d\n", already.outcome_resume_speed(), already.speed());
        return fail(21, "a city that was already paused resumed when the next scenario started");
    }

    CitySession broke;
    broke.new_city("Broke", 22);
    if (hostile_review_session_probe(broke, 14) != 100) {
        return fail(22, "could not open the untouched broke tax year");
    }
    const CitySession::BudgetBook open_book = broke.budget();
    if (open_book.road_note.find("cut to " + std::to_string(open_book.road_percent) + "%") == std::string::npos ||
        open_book.fire_note.find("cut to") == std::string::npos || !open_book.police_note.empty() ||
        open_book.road_percent >= 100 || broke.funds() != 100 || !dollars_match(open_book, 100)) {
        std::fprintf(stderr, "road '%s' fire '%s' police '%s' pct %d flow %ld current %ld\n",
                     open_book.road_note.c_str(), open_book.fire_note.c_str(), open_book.police_note.c_str(),
                     open_book.road_percent, open_book.cash_flow, open_book.funds);
        return fail(23, "an untouched broke budget hid the cut or mismatched the dollars");
    }
    broke.set_road_funding(0);
    broke.set_fire_funding(0);
    const CitySession::BudgetBook chosen_zero = broke.budget();
    if (!chosen_zero.road_note.empty() || !chosen_zero.fire_note.empty() || !dollars_match(chosen_zero, 100)) {
        std::fprintf(stderr, "zero notes '%s' '%s'\n", chosen_zero.road_note.c_str(), chosen_zero.fire_note.c_str());
        return fail(24, "funding the player set to zero still said the city cut it");
    }
    broke.set_road_funding(100);
    const CitySession::BudgetBook dragged = broke.budget();
    if (dragged.road_note.find("cut to") == std::string::npos || !dollars_match(dragged, 100)) {
        std::fprintf(stderr, "dragged '%s' flow %ld\n", dragged.road_note.c_str(), dragged.cash_flow);
        return fail(25, "raising the road slider above the treasury did not say it was cut");
    }
    const long projected = dragged.funds;
    broke.finish_budget_edit();
    if (broke.budget_pending() || broke.funds() != projected) {
        std::fprintf(stderr, "projected %ld actual %ld\n", projected, broke.funds());
        return fail(26, "closing the broke year did not post the dollars on screen");
    }

    CitySession menu_tax;
    menu_tax.new_city("Menu Tax", 23);
    if (hostile_review_session_probe(menu_tax, 15) != 2000 || menu_tax.budget_pending()) {
        return fail(27, "could not stage last January's taxes");
    }
    menu_tax.set_tax(20);
    if (menu_tax.tax() != 20 || menu_tax.budget().taxes != 2000) {
        std::fprintf(stderr, "menu tax %d collected %ld\n", menu_tax.tax(), menu_tax.budget().taxes);
        return fail(28, "a menu budget recomputed last January's taxes from the slider");
    }
    menu_tax.set_tax(0);
    if (menu_tax.budget().taxes != 2000) {
        return fail(29, "a menu tax of zero changed last January's receipt");
    }

    CitySession crossing;
    crossing.new_city("Crossing", 30);
    const ToolDef *rail = tool_named(TOOL_RAILROAD);
    const ToolDef *wire = tool_named(TOOL_WIRE);
    const ToolDef *road = tool_named(TOOL_ROAD);
    if (rail == nullptr || wire == nullptr || road == nullptr ||
        std::string(rail->hint).find("Rail bridge: $100") == std::string::npos ||
        std::string(wire->hint).find("Underwater wire: $25") == std::string::npos ||
        std::string(road->hint).find("Bridge: $50") == std::string::npos) {
        return fail(30, "a crossing hint does not include the water price");
    }
    if (hostile_review_session_probe(crossing, 12) != 0) {
        return fail(31, "could not stage rail across water");
    }
    const long rail_start = crossing.funds();
    const int rail_serial = crossing.message_serial();
    crossing.use_tool(TOOL_RAILROAD, 5, 5);
    if (crossing.funds() != rail_start - 20 || crossing.message().find("Rail bridge") != std::string::npos) {
        std::fprintf(stderr, "dry rail funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(32, "rail on land did not cost $20, or it claimed a bridge");
    }
    crossing.use_tool(TOOL_RAILROAD, 10, 12);
    if (crossing.funds() != rail_start - 120 || crossing.message().find("Rail bridge: $100") == std::string::npos ||
        crossing.message_serial() <= rail_serial) {
        std::fprintf(stderr, "h rail funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(33, "horizontal rail across water did not cost $100 or stay silent");
    }
    crossing.use_tool(TOOL_RAILROAD, 30, 16);
    if (crossing.funds() != rail_start - 220 || crossing.message().find("Rail bridge: $100") == std::string::npos) {
        std::fprintf(stderr, "v rail funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(34, "vertical rail across water did not cost $100");
    }
    if (hostile_review_session_probe(crossing, 13) != 0) {
        return fail(35, "could not stage power across water");
    }
    const long wire_start = crossing.funds();
    crossing.use_tool(TOOL_WIRE, 6, 6);
    if (crossing.funds() != wire_start - 5 || crossing.message().find("Underwater wire") != std::string::npos) {
        std::fprintf(stderr, "dry wire funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(36, "a power line on land did not cost $5, or it claimed water");
    }
    crossing.use_tool(TOOL_WIRE, 10, 14);
    if (crossing.funds() != wire_start - 30 || crossing.message().find("Underwater wire: $25") == std::string::npos) {
        std::fprintf(stderr, "h wire funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(37, "a horizontal power line across water did not cost $25 or stay silent");
    }
    crossing.use_tool(TOOL_WIRE, 14, 10);
    if (crossing.funds() != wire_start - 55 || crossing.message().find("Underwater wire: $25") == std::string::npos) {
        std::fprintf(stderr, "v wire funds %ld msg '%s'\n", crossing.funds(), crossing.message().c_str());
        return fail(38, "a vertical power line across water did not cost $25");
    }
    if (hostile_review_session_probe(crossing, 11) != 0) {
        return fail(39, "could not stage open water");
    }
    const long missed = crossing.funds();
    const bool dirty_before = crossing.needs_save_prompt();
    crossing.use_tool(TOOL_RAILROAD, 20, 20);
    const std::string rail_miss = crossing.message();
    crossing.use_tool(TOOL_WIRE, 20, 20);
    if (crossing.funds() != missed || rail_miss.find("anchor") == std::string::npos ||
        crossing.message().find("anchor") == std::string::npos || crossing.needs_save_prompt() != dirty_before) {
        std::fprintf(stderr, "miss rail '%s' wire '%s' funds %ld\n", rail_miss.c_str(), crossing.message().c_str(),
                     crossing.funds());
        return fail(40, "rail or power with nowhere to anchor was silent or charged the city");
    }

    CitySession saved;
    saved.new_city("Reload", 31);
    const std::string reload_path = "/tmp/lunduke-round4-reload.cty";
    if (!saved.save_city_as(reload_path)) {
        return fail(41, "could not save the city used for the load message");
    }
    const int serial_before = saved.message_serial();
    if (!saved.load_city(reload_path) || saved.message() != "Loaded a saved city." ||
        saved.message_serial() <= serial_before) {
        std::fprintf(stderr, "load msg '%s' serial %d %d\n", saved.message().c_str(), serial_before,
                     saved.message_serial());
        return fail(42, "loading a city did not post a new 'Loaded a saved city.'");
    }
    const int serial_once = saved.message_serial();
    if (!saved.load_city(reload_path) || saved.message_serial() <= serial_once) {
        return fail(43, "loading the same city again reused the old message serial");
    }
    const int scenario_serial = saved.message_serial();
    if (!saved.load_scenario(SC_DULLSVILLE) || saved.message() != "Playing Dullsville." ||
        saved.message_serial() <= scenario_serial) {
        std::fprintf(stderr, "play msg '%s'\n", saved.message().c_str());
        return fail(44, "starting a scenario did not post 'Playing Dullsville.'");
    }

    std::remove(path.c_str());
    std::remove(reload_path.c_str());
    return 0;
}

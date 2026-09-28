// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless checks for the 0.2 bridge: budget funding, overlay samples,
// sprite state, and sound startup. No window is opened.

#include "city_session.hpp"
#include "sound_player.hpp"
#include "tools.hpp"
#include "version.hpp"
#include "zoom_keys.hpp"

#include "micropolis.h"

#include <cstdio>
#include <string>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

} // namespace

int main()
{
    CitySession session;
    session.new_city("Feature Check", 42);

    session.set_road_funding(40);
    session.set_police_funding(0);
    session.set_fire_funding(100);
    session.set_tax(9);
    const CitySession::BudgetBook book = session.budget();
    if (book.road_percent != 40 || book.police_percent != 0 || book.fire_percent != 100) {
        return fail(1, "funding percents were not stored");
    }
    if (book.tax_percent != 9) {
        return fail(2, "tax rate was not stored");
    }

    session.set_road_funding(250);
    session.set_fire_funding(-3);
    const CitySession::BudgetBook clamped = session.budget();
    if (clamped.road_percent != 100 || clamped.fire_percent != 0) {
        return fail(3, "funding percents were not clamped");
    }

    int water = 0;
    int checked = 0;
    for (int y = 0; y < CitySession::kWorldH; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int tile = session.map_value(x, y) & LOMASK;
            const bool expect = (tile >= RIVER && tile <= WATER_HIGH) || (tile >= FLOOD && tile <= LASTFLOOD);
            const int got = session.layer_value(CitySession::MapLayer::Water, x, y);
            if ((got > 0) != expect) {
                return fail(4, "water overlay does not match map tiles");
            }
            water += got;
            const int pollution = session.layer_value(CitySession::MapLayer::Pollution, x, y);
            const int crime = session.layer_value(CitySession::MapLayer::Crime, x, y);
            const int land = session.layer_value(CitySession::MapLayer::LandValue, x, y);
            const int traffic = session.layer_value(CitySession::MapLayer::Traffic, x, y);
            const int power = session.layer_value(CitySession::MapLayer::Power, x, y);
            if (pollution < 0 || crime < 0 || land < 0 || traffic < 0 || power < 0) {
                return fail(5, "overlay sample was negative");
            }
            ++checked;
        }
    }
    if (water < 50 || checked != CitySession::kWorldW * CitySession::kWorldH) {
        return fail(6, "expected a generated city to contain water");
    }

    session.set_speed(3);
    for (int i = 0; i < 8; ++i) {
        session.tick();
    }
    (void)session.layer_value(CitySession::MapLayer::LandValue, 10, 10);
    (void)session.layer_value(CitySession::MapLayer::Power, 10, 10);

    int origin_x = 0;
    int origin_y = 0;
    if (!session.stamp_neighborhood(origin_x, origin_y)) {
        return fail(7, "could not place a neighborhood on clear land");
    }
    bool residential = false;
    for (int y = 0; y < CitySession::kWorldH && !residential; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int tile = session.map_value(x, y) & LOMASK;
            if (tile >= RESBASE && tile < COMBASE) {
                residential = true;
                break;
            }
        }
    }
    if (!residential) {
        return fail(8, "neighborhood did not place a residential zone");
    }

    session.place_sprite(SPRITE_AIRPLANE, origin_x + 8, origin_y + 4);
    const auto sprites = session.sprites();
    bool airplane = false;
    for (const auto &dot : sprites) {
        if (dot.type == SPRITE_AIRPLANE && dot.frame > 0 && dot.width > 0) {
            airplane = true;
        }
    }
    if (!airplane) {
        return fail(9, "airplane sprite was not reported");
    }

    SoundPlayer player;
    player.probe();
    player.set_muted(true);
    if (player.play("Siren")) {
        return fail(10, "muted player should not start playback");
    }
    player.play("not-a-real-sound");
    player.set_muted(false);
    player.play("Beep");

    const ToolFootprint res_foot = tool_footprint(TOOL_RESIDENTIAL);
    const ToolFootprint commercial = tool_footprint(TOOL_COMMERCIAL);
    const ToolFootprint industrial = tool_footprint(TOOL_INDUSTRIAL);
    const ToolFootprint stadium = tool_footprint(TOOL_STADIUM);
    const ToolFootprint airport = tool_footprint(TOOL_AIRPORT);
    const ToolFootprint road = tool_footprint(TOOL_ROAD);
    const ToolFootprint rail = tool_footprint(TOOL_RAILROAD);
    const ToolFootprint wire = tool_footprint(TOOL_WIRE);
    const ToolFootprint park = tool_footprint(TOOL_PARK);
    const ToolFootprint dozer = tool_footprint(TOOL_BULLDOZER);
    const ToolFootprint query = tool_footprint(TOOL_QUERY);
    if (!res_foot.placeable || res_foot.width != 3 || res_foot.height != 3 ||
        res_foot.cursor_to_left != 1 || res_foot.cursor_to_top != 1 ||
        commercial.width != 3 || industrial.width != 3) {
        return fail(11, "zone footprint is not the engine 3x3 center");
    }
    if (stadium.width != 4 || stadium.height != 4 || airport.width != 6 || airport.height != 6 ||
        airport.cursor_to_left != 1) {
        return fail(12, "building footprint does not match gToolSize");
    }
    if (!road.placeable || road.width != 1 || rail.width != 1 || wire.width != 1 || park.width != 1 ||
        dozer.width != 1 || road.cursor_to_left != 0) {
        return fail(13, "line tools should be a single tile on the cursor");
    }
    if (query.placeable) {
        return fail(14, "query should not draw a placement preview");
    }

    // Ctrl and the +/= key (GDK_KEY_equal) zooms in with no Shift bit.
    constexpr unsigned kControl = 4;
    constexpr unsigned kShift = 1;
    if (zoom_action(0x03d, kControl) != ZoomAction::In ||
        zoom_action(0x03d, kControl | kShift) != ZoomAction::In ||
        zoom_action(0x02b, kControl) != ZoomAction::In ||
        zoom_action(0xffab, kControl) != ZoomAction::In) {
        return fail(15, "ctrl-+ should zoom in without requiring shift");
    }
    if (zoom_action(0x02d, kControl) != ZoomAction::Out || zoom_action(0xffad, kControl) != ZoomAction::Out) {
        return fail(16, "ctrl-minus should still zoom out");
    }
    if (zoom_action(0x03d, 0) != ZoomAction::None || zoom_action(0x03d, kControl | 8) != ZoomAction::None) {
        return fail(17, "zoom should ignore the bare equal key and ctrl-alt");
    }

    if (CitySession::kScenarioCount != 8) {
        return fail(18, "expected the eight engine scenarios");
    }
    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        if (def.name == nullptr || def.name[0] == '\0' || def.year < 1900) {
            return fail(19, "scenario catalog entry is incomplete");
        }
        if (!session.load_scenario(def.id) || session.city_name() != def.name) {
            return fail(20, "scenario did not load through the engine");
        }
        if (!session.save_path().empty()) {
            return fail(21, "a scenario should not keep a save path");
        }
    }
    if (!session.load_scenario(CitySession::scenario_def(0).id) ||
        session.city_name() != "Dullsville" ||
        session.funds_text().find("$5,000") == std::string::npos) {
        return fail(22, "Dullsville should start with $5,000");
    }

    if (std::string(kPackageVersion) != "0.7-3" || std::string(kReleaseTrack) != "0.7") {
        return fail(23, "package version should be the 0.7-3 identity");
    }

    auto count_kind = [](CitySession &city, bool woods) {
        int count = 0;
        for (int y = 0; y < CitySession::kWorldH; ++y) {
            for (int x = 0; x < CitySession::kWorldW; ++x) {
                const int tile = city.map_value(x, y) & LOMASK;
                const bool water =
                    (tile >= RIVER && tile <= WATER_HIGH) || (tile >= FLOOD && tile <= LASTFLOOD);
                if (woods ? tile == WOODS : water) {
                    ++count;
                }
            }
        }
        return count;
    };
    auto map_hash = [](CitySession &city) {
        unsigned hash = 2166136261u;
        for (int y = 0; y < CitySession::kWorldH; ++y) {
            for (int x = 0; x < CitySession::kWorldW; ++x) {
                hash ^= static_cast<unsigned>(city.map_value(x, y));
                hash *= 16777619u;
            }
        }
        return hash;
    };

    CitySession::NewCitySpec dry;
    dry.name = "Dryland";
    dry.seed = 7;
    dry.difficulty = CitySession::kLevelHard;
    dry.island = CitySession::kTerrainOff;
    dry.rivers = CitySession::kTerrainOff;
    dry.lakes = CitySession::kTerrainOff;
    dry.trees = CitySession::kTerrainOff;
    session.new_city(dry);
    if (session.funds() != 5000 || session.difficulty() != CitySession::kLevelHard ||
        session.generated_seed() != 7 || session.city_name() != "Dryland") {
        return fail(24, "hard new city did not apply level, seed, or name");
    }
    if (count_kind(session, false) != 0 || count_kind(session, true) != 0) {
        return fail(25, "terrain-off city should have no water and no woods");
    }

    CitySession::NewCitySpec river = dry;
    river.rivers = CitySession::kRiversCurvy;
    river.seed = 11;
    session.new_city(river);
    if (count_kind(session, false) < 10) {
        return fail(26, "curvier river setting produced no river");
    }

    CitySession::NewCitySpec lakes = dry;
    lakes.lakes = CitySession::kLakesMany;
    lakes.seed = 13;
    session.new_city(lakes);
    if (count_kind(session, false) < 10) {
        return fail(27, "many-lakes setting produced no water");
    }

    CitySession::NewCitySpec wooded = dry;
    wooded.trees = CitySession::kTreesWooded;
    wooded.seed = 17;
    session.new_city(wooded);
    if (count_kind(session, true) < 20) {
        return fail(28, "wooded setting produced no trees");
    }

    CitySession::NewCitySpec medium;
    medium.name = "Middle";
    medium.seed = 3;
    medium.difficulty = CitySession::kLevelMedium;
    session.new_city(medium);
    if (session.funds() != 10000 || session.difficulty() != CitySession::kLevelMedium) {
        return fail(29, "medium difficulty should start with $10,000");
    }
    const unsigned first_map = map_hash(session);
    medium.seed = 99;
    session.new_city(medium);
    if (session.generated_seed() != 99 || map_hash(session) == first_map) {
        return fail(30, "a different seed should build a different map");
    }
    session.new_city("Easy Check", 5);
    if (session.funds() != 20000 || session.difficulty() != CitySession::kLevelEasy ||
        session.generated_seed() != 5) {
        return fail(31, "easy difficulty should start with $20,000");
    }

    if (session.history_value(CitySession::HistorySeries::CashFlow, CitySession::HistoryScale::Short, 0) !=
            128 ||
        session.history_value(CitySession::HistorySeries::Residential, CitySession::HistoryScale::Short, 0) !=
            0 ||
        session.history_value(CitySession::HistorySeries::Pollution, CitySession::HistoryScale::Long, 0) != 0 ||
        session.history_value(CitySession::HistorySeries::Crime, CitySession::HistoryScale::Short, 999) != 0) {
        return fail(32, "history tables were not the fresh-city census");
    }

    session.update_evaluation();
    const CitySession::Evaluation report = session.evaluation();
    if (report.score < 0 || report.score > 1000 || report.yes_percent < 0 || report.yes_percent > 100) {
        return fail(33, "evaluation score or opinion is out of range");
    }
    if (report.category != "Village" || report.difficulty != "Easy" || report.problems.empty()) {
        return fail(34, "evaluation did not report class, difficulty, and problems");
    }
    for (const auto &problem : report.problems) {
        if (problem.name.empty() || problem.votes < 0 || problem.votes > 100) {
            return fail(35, "problem row was not engine data");
        }
    }

    session.rename_city("  Harbor   Town ");
    if (session.city_name() != "Harbor Town") {
        return fail(36, "rename did not store the cleaned city name");
    }
    session.rename_city("   ");
    if (session.city_name() != "Harbor Town") {
        return fail(37, "a blank rename should leave the current name");
    }

    // A paused tick must still advance an animated tile. simTick() does
    // not do that; CitySession::tick() has to call animateTiles(), the
    // same follow-up upstream Micropolis front ends use. Park placement
    // drops a fountain (the animated tile) on one try in five.
    CitySession animated;
    animated.new_city("Fountain", 21);
    animated.set_speed(0);
    animated.set_disasters(false);
    int fountain_x = -1;
    int fountain_y = -1;
    int planted = 0;
    for (int y = 0; y < CitySession::kWorldH && fountain_x < 0 && planted < 80; ++y) {
        for (int x = 0; x < CitySession::kWorldW && planted < 80; ++x) {
            if ((animated.map_value(x, y) & LOMASK) != DIRT) {
                continue;
            }
            animated.use_tool(TOOL_PARK, x, y);
            ++planted;
            if ((animated.map_value(x, y) & LOMASK) == FOUNTAIN &&
                (animated.map_value(x, y) & ANIMBIT) != 0) {
                fountain_x = x;
                fountain_y = y;
            }
        }
    }
    if (fountain_x < 0) {
        return fail(38, "park tool did not place an animated fountain");
    }
    const int before = animated.map_value(fountain_x, fountain_y);
    const int before_tile = before & LOMASK;
    const int next_tile = Micropolis::getNextAnimatedTile(before_tile);
    if (next_tile < 0 || next_tile == before_tile) {
        return fail(39, "fountain tile is not in the animation table");
    }
    animated.tick();
    const int after = animated.map_value(fountain_x, fountain_y);
    if ((after & LOMASK) != next_tile || (after & ALLBITS) != (before & ALLBITS)) {
        return fail(40, "tick did not animate the fountain");
    }
    const int next_again = Micropolis::getNextAnimatedTile(next_tile);
    animated.tick();
    if ((animated.map_value(fountain_x, fountain_y) & LOMASK) != next_again) {
        return fail(41, "a second tick did not advance the fountain");
    }
    return 0;
}

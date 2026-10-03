// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cstdarg>
#include <functional>
#include <string>
#include <vector>

// Thin bridge over the vendored Micropolis engine. The UI never talks to
// Tcl, and it does not include the engine header.
class CitySession {
public:
    static constexpr int kWorldW = 120;
    static constexpr int kWorldH = 100;

    struct SpriteDot {
        int type = 0;
        int frame = 0;
        int x = 0;
        int y = 0;
        int x_offset = 0;
        int y_offset = 0;
        int width = 0;
        int height = 0;
        int tile_x = 0;
        int tile_y = 0;
    };

    struct BudgetBook {
        long taxes = 0;
        long cash_flow = 0;
        long funds = 0;
        long previous_funds = 0;
        long road_need = 0;
        long police_need = 0;
        long fire_need = 0;
        long road_spent = 0;
        long police_spent = 0;
        long fire_spent = 0;
        int tax_percent = 0;
        int road_percent = 100;
        int police_percent = 100;
        int fire_percent = 100;
    };

    // Separate map views. Power codes: 0 empty, 2 unpowered zone,
    // 3 powered zone, 4 conductive line. Water is 0 or 1. The density
    // layers return the engine's cluster value (0..255).
    enum class MapLayer {
        Power,
        Water,
        Pollution,
        Crime,
        LandValue,
        Traffic,
    };

    using Listener = std::function<void()>;

    CitySession();
    ~CitySession();

    CitySession(const CitySession &) = delete;
    CitySession &operator=(const CitySession &) = delete;

    void set_listener(Listener listener);

    // Pre-made scenarios the engine loads from res/snro.* (Dullsville
    // through Rio de Janeiro). id matches the engine Scenario ordinal.
    struct ScenarioDef {
        int id = 0;
        const char *name = "";
        int year = 0;
        const char *summary = "";
    };

    static constexpr int kScenarioCount = 8;
    static const ScenarioDef &scenario_def(int index);

    // Difficulty is the engine GameLevel: easy $20,000, medium $10,000,
    // hard $5,000. It also changes the tax and road-cost tables.
    static constexpr int kLevelEasy = 0;
    static constexpr int kLevelMedium = 1;
    static constexpr int kLevelHard = 2;

    // Terrain knobs are the engine fields used by generateMap().
    // -1 is the generator default. 0 turns that feature off.
    // Island 1 always builds an island (the only other value the engine reads).
    static constexpr int kTerrainDefault = -1;
    static constexpr int kTerrainOff = 0;
    static constexpr int kIslandAlways = 1;
    // Curve level above 0 makes rivers turn more often than the default rates.
    static constexpr int kRiversCurvy = 200;
    // Non-negative lake level is a lake count times two (20 => 10 lakes).
    static constexpr int kLakesMany = 20;
    // Non-negative tree level: splash count is level + 3 (197 => 200 splashes).
    static constexpr int kTreesWooded = 197;

    struct NewCitySpec {
        std::string name;
        int seed = 0;
        // False with seed 0 means "pick from the clock", which is what the
        // two-argument new_city() still does. True uses seed as written,
        // including 0.
        bool seed_was_set = false;
        int difficulty = kLevelEasy;
        int island = kTerrainDefault;
        int rivers = kTerrainDefault;
        int lakes = kTerrainDefault;
        int trees = kTerrainDefault;
    };

    // seed 0 asks the engine path to draw a seed from the clock.
    // The two-argument form keeps the historical easy / default-terrain city.
    void new_city(const std::string &name, int seed = 0);
    void new_city(const NewCitySpec &spec);
    bool load_city(const std::string &path);
    bool save_city_as(const std::string &path);
    bool load_scenario(int id);
    void rename_city(const std::string &name);

    int difficulty() const;
    long funds() const;
    int generated_seed() const;

    void tick();
    void use_tool(int engine_tool, int tile_x, int tile_y);
    void drag_tool(int engine_tool, int from_x, int from_y, int to_x, int to_y);

    void set_speed(int speed);
    int speed() const;

    void set_auto_budget(bool on);
    bool auto_budget() const;
    void set_auto_bulldoze(bool on);
    bool auto_bulldoze() const;
    void set_disasters(bool on);
    bool disasters() const;
    void set_auto_goto(bool on);
    bool auto_goto() const;
    void set_tax(int percent);
    int tax() const;
    void set_road_funding(int percent);
    void set_police_funding(int percent);
    void set_fire_funding(int percent);
    BudgetBook budget() const;

    void set_sound_enabled(bool on);
    bool sound_enabled() const;
    std::vector<std::string> take_sounds();
    bool take_budget_request();
    // Charge a tax year that was waiting on the budget window. A no-op
    // when nothing is waiting, including a second close.
    void commit_pending_budget();
    // Tile the engine asked the view to center on (auto-goto).
    bool take_view_target(int &tile_x, int &tile_y);
    // Strength of a quake the engine just started, 0 if none is waiting.
    int take_earthquake();

    // Drop a mobile sprite on a tile. Used so the map can show the
    // engine's sprite list without waiting for the simulator to spawn one.
    void place_sprite(int type, int tile_x, int tile_y);

    // Lay a small powered neighborhood on clear land, for a visible demo.
    bool stamp_neighborhood(int &origin_x, int &origin_y);

    void disaster_fire();
    void disaster_flood();
    void disaster_tornado();
    void disaster_earthquake();
    void disaster_monster();
    void disaster_meltdown();

    std::string city_name() const;
    std::string funds_text() const;
    std::string date_text() const;
    std::string message() const;
    // Bumps once per query-tool report so the window can open feedback
    // even when the text matches the previous tile.
    int query_serial() const { return query_serial_; }
    std::string evaluation_text();
    std::string budget_text() const;

    // Engine history tables. Index 0 is the newest sample.
    // Short is 120 monthly samples (10 years). Long is 120 yearly samples.
    enum class HistorySeries {
        Residential = 0,
        Commercial,
        Industrial,
        CashFlow,
        Crime,
        Pollution,
    };
    enum class HistoryScale {
        Short = 0,
        Long = 1,
    };
    static constexpr int kHistoryPoints = 120;
    int history_value(HistorySeries series, HistoryScale scale, int index) const;

    // Public-opinion and score data from the last cityEvaluation().
    // update_evaluation() runs that pass; evaluation() only reads it.
    struct Problem {
        std::string name;
        int votes = 0;
    };
    struct Evaluation {
        int year = 0;
        int score = 0;
        int score_delta = 0;
        int yes_percent = 0;
        long population = 0;
        long migration = 0;
        long assessed_value = 0;
        std::string category;
        std::string difficulty;
        std::vector<Problem> problems;
    };
    void update_evaluation();
    Evaluation evaluation() const;

    // Demand in the range -1..1 (shortage to surplus is negative..positive).
    double res_demand();
    double com_demand();
    double ind_demand();

    // Raw map word (tile character in the low bits, flags above).
    int map_value(int x, int y) const;
    int layer_value(MapLayer layer, int x, int y) const;
    unsigned map_serial() const;
    std::vector<SpriteDot> sprites() const;

    const std::string &save_path() const { return save_path_; }

private:
    struct Engine;

    void on_callback(const char *name, const char *params, va_list args);
    void notify();

    Engine *engine_;
    Listener listener_;
    void set_service_funding(int kind, int percent);

    std::string message_;
    int query_serial_ = 0;
    std::string save_path_;
    std::vector<std::string> sounds_;
    bool budget_requested_ = false;
    bool goto_pending_ = false;
    int goto_x_ = 0;
    int goto_y_ = 0;
    int quake_strength_ = 0;
    bool sound_enabled_ = true;
    bool ready_ = false;
    int speed_ = 2;
};

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
        // Set when Close will fund a department below the slider, because
        // tax plus cash cannot cover the request.
        std::string road_note;
        std::string police_note;
        std::string fire_note;
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
    // False when the name is empty after spaces, tabs, and controls are stripped.
    static bool name_is_usable(const std::string &name);

    int difficulty() const;
    long funds() const;
    int generated_seed() const;

    void tick();
    void use_tool(int engine_tool, int tile_x, int tile_y);
    void drag_tool(int engine_tool, int from_x, int from_y, int to_x, int to_y);

    void set_speed(int speed);
    int speed() const;
    // Modal dialogs pause simTick and tile animation until they return.
    void set_simulation_paused(bool paused);
    bool simulation_paused() const;
    // year * 12 + month, used to refresh evaluation about once a month.
    int game_month_index() const;

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
    // Put the sliders back to the rates captured when the book opened.
    // Does not collect a waiting tax year and does not close the book.
    void restore_budget_rates(int tax_percent, int road_percent, int police_percent, int fire_percent);
    BudgetBook budget() const;

    void set_sound_enabled(bool on);
    bool sound_enabled() const;
    std::vector<std::string> take_sounds();
    bool take_budget_request();
    // A modal dialog deferred the budget window. Ask for it again.
    void keep_budget_request();
    // Charge a tax year that was waiting on the budget window. A no-op
    // when nothing is waiting, including a second close.
    void commit_pending_budget();
    // True while a tax year is waiting for the budget window.
    bool budget_pending() const;
    // Drop a tax year without charging it. Used when the city is replaced.
    void discard_pending_budget();
    // Commit a waiting tax year, or apply funding effects when the book
    // was opened from the menu. Road effects stay put until this runs.
    void finish_budget_edit();
    // Live census is a finished scan (phase 9 or later, or just generated).
    bool census_ready() const;
    // Refresh the evaluation preview when the displayed month changes and
    // the census is complete. Returns false when the pass was skipped.
    bool note_evaluation_month(int month_index);
    // cityAssessedValue as stored on the engine, not the preview copy.
    long stored_assessed_value() const;
    // Unsaved tools, budget, disasters, rename, or a tax year still open.
    bool needs_save_prompt() const;
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
    // Increases once per engine notice, including a repeat of the same words.
    int message_serial() const { return message_serial_; }
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
    // Dollars for one cash-flow sample. Index 0 is the newest.
    long cash_flow_history(HistoryScale scale, int index) const;
    // False when the sample was reconstructed from the capped history byte.
    bool cash_flow_history_exact(HistoryScale scale, int index) const;
    // 1 when the scenario was just won, -1 when it was just lost, then 0.
    int take_scenario_outcome();
    // Speed the city was running at when the last win or loss paused it.
    int outcome_resume_speed() const { return speed_before_outcome_; }
    // True after a win or loss until Keep playing, a new scenario, or a
    // chosen running speed consumes that pause.
    bool outcome_pause_pending() const { return outcome_paused_; }
    // Leave the announcement pause and run at outcome_resume_speed().
    void resume_after_outcome();

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
    void post_message(std::string text);
    void mark_dirty();

    std::string message_;
    int message_serial_ = 0;
    int scenario_outcome_ = 0;
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
    bool simulation_paused_ = false;
    int speed_ = 2;
    bool dirty_ = false;
    bool in_callback_ = false;
    bool eval_month_pending_ = false;
    int eval_month_ = -1;
    int speed_before_outcome_ = 2;
    bool outcome_paused_ = false;

    friend int hostile_review_session_probe(CitySession &session, int op);
};

// Test hook for the census month and a budget left open across New City.
int hostile_review_session_probe(CitySession &session, int op);

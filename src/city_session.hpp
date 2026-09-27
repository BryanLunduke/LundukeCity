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

    void new_city(const std::string &name, int seed = 0);
    bool load_city(const std::string &path);
    bool save_city_as(const std::string &path);

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
    std::string evaluation_text();
    std::string budget_text() const;

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
    std::string save_path_;
    std::vector<std::string> sounds_;
    bool budget_requested_ = false;
    bool sound_enabled_ = true;
    bool ready_ = false;
    int speed_ = 2;
};

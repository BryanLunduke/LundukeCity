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
        int tile_x = 0;
        int tile_y = 0;
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
    unsigned map_serial() const;
    std::vector<SpriteDot> sprites() const;

    const std::string &save_path() const { return save_path_; }

private:
    struct Engine;

    void on_callback(const char *name, const char *params, va_list args);
    void notify();

    Engine *engine_;
    Listener listener_;
    std::string message_;
    std::string save_path_;
    bool ready_ = false;
    int speed_ = 2;
};

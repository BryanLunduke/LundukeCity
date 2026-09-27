// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "city_session.hpp"

#include "messages.hpp"

#include "micropolis.h"

#include <algorithm>
#include <cstdarg>
#include <ctime>
#include <memory>
#include <sstream>

namespace {

std::string with_commas(long value)
{
    const bool neg = value < 0;
    unsigned long mag = static_cast<unsigned long>(neg ? -value : value);
    std::string digits = std::to_string(mag);
    std::string grouped;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
    }
    std::reverse(grouped.begin(), grouped.end());
    return (neg ? "-$" : "$") + grouped;
}

const char *kMonths[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

const char *city_class_name(CityClass city_class)
{
    switch (city_class) {
    case CC_VILLAGE:
        return "Village";
    case CC_TOWN:
        return "Town";
    case CC_CITY:
        return "City";
    case CC_CAPITAL:
        return "Capital";
    case CC_METROPOLIS:
        return "Metropolis";
    case CC_MEGALOPOLIS:
        return "Megalopolis";
    default:
        return "Settlement";
    }
}

} // namespace

// Held out of the header so the UI translation units do not include the engine.
struct CitySession::Engine {
    Micropolis sim;
};

CitySession::CitySession()
    : engine_(new Engine)
{
    engine_->sim.callbackHook = [](Micropolis * /*sim*/, void *data, const char *name,
                                   const char *params, va_list args) {
        auto *self = static_cast<CitySession *>(data);
        if (self == nullptr) {
            return;
        }
        self->on_callback(name, params, args);
    };
    engine_->sim.callbackData = this;
    engine_->sim.setEnableSound(sound_enabled_);
}

CitySession::~CitySession()
{
    engine_->sim.callbackHook = nullptr;
    engine_->sim.callbackData = nullptr;
    delete engine_;
}

void CitySession::set_listener(Listener listener)
{
    listener_ = std::move(listener);
}

void CitySession::notify()
{
    if (listener_) {
        listener_();
    }
}

void CitySession::on_callback(const char *name, const char *params, va_list args)
{
    const std::string which = name != nullptr ? name : "";

    auto take_string = [&]() -> const char * {
        const char *s = va_arg(args, char *);
        return s != nullptr ? s : "";
    };
    auto take_int = [&]() { return va_arg(args, int); };

    if (which == "update" && params != nullptr && params[0] == 's') {
        const std::string kind = take_string();
        if (kind == "message") {
            int number = 0;
            const char *p = params + 1;
            if (*p == 'd') {
                number = take_int();
            }
            message_ = message_for_number(number);
            notify();
        }
        return;
    }

    if (which == "showZoneStatus") {
        int category = 0;
        int s0 = 0, s1 = 0, s2 = 0, s3 = 0, s4 = 0;
        const char *p = params != nullptr ? params : "";
        if (*p == 'd') {
            category = take_int();
            ++p;
        }
        if (*p == 'd') {
            s0 = take_int();
            ++p;
        }
        if (*p == 'd') {
            s1 = take_int();
            ++p;
        }
        if (*p == 'd') {
            s2 = take_int();
            ++p;
        }
        if (*p == 'd') {
            s3 = take_int();
            ++p;
        }
        if (*p == 'd') {
            s4 = take_int();
        }
        message_ = zone_status_text(category, s0, s1, s2, s3, s4);
        notify();
        return;
    }

    if (which == "makeSound") {
        const char *channel = (params != nullptr && params[0] == 's') ? take_string() : "";
        const char *sound = (params != nullptr && params[0] == 's' && params[1] == 's') ? take_string() : "";
        (void)channel;
        if (sound[0] != '\0') {
            sounds_.emplace_back(sound);
        }
        return;
    }

    if (which == "showBudgetAndWait") {
        budget_requested_ = true;
        notify();
        return;
    }

    if (which == "didntLoadCity" || which == "didntSaveCity") {
        const char *msg = (params != nullptr && params[0] == 's') ? take_string() : "";
        message_ = std::string(which == "didntLoadCity" ? "Could not load " : "Could not save ") + msg;
        notify();
    }
}

void CitySession::new_city(const std::string &name, int seed)
{
    ready_ = false;
    Micropolis &sim = engine_->sim;
    // Micropolis::init() (called from the constructor) already ran simInit().
    // simInit() is private and reallocates history buffers, so a new city
    // resets funds and options here and lets generateSomeCity() rebuild the map.
    sim.setGameLevelFunds(LEVEL_EASY);
    sim.setCityTax(7);
    sim.setAutoBudget(true);
    sim.setAutoBulldoze(true);
    sim.setEnableDisasters(true);
    const std::string city = name.empty() ? "New City" : name;
    sim.setCleanCityName(city);
    sim.setSpeed(static_cast<short>(speed_));
    sim.setPasses(1);
    const int used_seed = seed != 0 ? seed : static_cast<int>(std::time(nullptr));
    sim.generateSomeCity(used_seed);
    sim.setSpeed(static_cast<short>(speed_));
    sim.setEnableSound(sound_enabled_);
    save_path_.clear();
    message_.clear();
    ready_ = true;
    notify();
}

bool CitySession::load_city(const std::string &path)
{
    Micropolis &sim = engine_->sim;
    if (!sim.loadCity(path.c_str())) {
        message_ = "Could not load that city file.";
        notify();
        return false;
    }
    sim.setSpeed(static_cast<short>(speed_));
    sound_enabled_ = sim.enableSound;
    save_path_ = path;
    ready_ = true;
    if (message_.empty()) {
        message_ = "Loaded a saved city.";
    }
    notify();
    return true;
}

bool CitySession::save_city_as(const std::string &path)
{
    message_.clear();
    // saveCityAs reports failure through the callback hook.
    engine_->sim.saveCityAs(path.c_str());
    if (message_.rfind("Could not save", 0) == 0) {
        return false;
    }
    save_path_ = path;
    message_ = "City saved.";
    notify();
    return true;
}

void CitySession::tick()
{
    if (!ready_) {
        return;
    }
    engine_->sim.simTick();
}

void CitySession::use_tool(int engine_tool, int tile_x, int tile_y)
{
    if (!ready_) {
        return;
    }
    engine_->sim.toolDown(static_cast<EditingTool>(engine_tool),
                          static_cast<short>(tile_x), static_cast<short>(tile_y));
    notify();
}

void CitySession::drag_tool(int engine_tool, int from_x, int from_y, int to_x, int to_y)
{
    if (!ready_) {
        return;
    }
    engine_->sim.toolDrag(static_cast<EditingTool>(engine_tool),
                          static_cast<short>(from_x), static_cast<short>(from_y),
                          static_cast<short>(to_x), static_cast<short>(to_y));
    notify();
}

void CitySession::set_speed(int speed)
{
    speed_ = std::max(0, std::min(3, speed));
    if (ready_) {
        engine_->sim.setSpeed(static_cast<short>(speed_));
    }
}

int CitySession::speed() const
{
    return speed_;
}

void CitySession::set_auto_budget(bool on)
{
    engine_->sim.setAutoBudget(on);
}

bool CitySession::auto_budget() const
{
    return engine_->sim.autoBudget;
}

void CitySession::set_auto_bulldoze(bool on)
{
    engine_->sim.setAutoBulldoze(on);
}

bool CitySession::auto_bulldoze() const
{
    return engine_->sim.autoBulldoze;
}

void CitySession::set_disasters(bool on)
{
    engine_->sim.setEnableDisasters(on);
}

bool CitySession::disasters() const
{
    return engine_->sim.enableDisasters;
}

void CitySession::set_tax(int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 20) {
        percent = 20;
    }
    engine_->sim.setCityTax(static_cast<short>(percent));
}

int CitySession::tax() const
{
    return engine_->sim.cityTax;
}

void CitySession::set_service_funding(int kind, int percent)
{
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    const float fraction = static_cast<float>(percent) / 100.0f;
    Micropolis &sim = engine_->sim;
    Quad *fund = &sim.roadFund;
    float *slot = &sim.roadPercent;
    Quad *spend = &sim.roadSpend;
    if (kind == 1) {
        fund = &sim.policeFund;
        slot = &sim.policePercent;
        spend = &sim.policeSpend;
    } else if (kind == 2) {
        fund = &sim.fireFund;
        slot = &sim.firePercent;
        spend = &sim.fireSpend;
    }
    *slot = fraction;
    *spend = static_cast<Quad>(*fund * fraction);
    sim.updateFundEffects();
}

void CitySession::set_road_funding(int percent)
{
    set_service_funding(0, percent);
}

void CitySession::set_police_funding(int percent)
{
    set_service_funding(1, percent);
}

void CitySession::set_fire_funding(int percent)
{
    set_service_funding(2, percent);
}

namespace {

int percent_of(float fraction)
{
    int n = static_cast<int>(fraction * 100.0f + 0.5f);
    if (n < 0) {
        n = 0;
    }
    if (n > 100) {
        n = 100;
    }
    return n;
}

long funded(Quad need, int percent)
{
    return static_cast<long>(need) * percent / 100;
}

} // namespace

CitySession::BudgetBook CitySession::budget() const
{
    const Micropolis &sim = engine_->sim;
    BudgetBook book;
    book.taxes = static_cast<long>(sim.taxFund);
    book.funds = static_cast<long>(sim.totalFunds);
    book.tax_percent = sim.cityTax;
    book.road_percent = percent_of(sim.roadPercent);
    book.police_percent = percent_of(sim.policePercent);
    book.fire_percent = percent_of(sim.firePercent);
    book.road_need = static_cast<long>(sim.roadFund);
    book.police_need = static_cast<long>(sim.policeFund);
    book.fire_need = static_cast<long>(sim.fireFund);
    book.road_spent = funded(sim.roadFund, book.road_percent);
    book.police_spent = funded(sim.policeFund, book.police_percent);
    book.fire_spent = funded(sim.fireFund, book.fire_percent);
    book.cash_flow = book.taxes - book.road_spent - book.police_spent - book.fire_spent;
    return book;
}

void CitySession::set_sound_enabled(bool on)
{
    sound_enabled_ = on;
    engine_->sim.setEnableSound(on);
}

bool CitySession::sound_enabled() const
{
    return sound_enabled_;
}

std::vector<std::string> CitySession::take_sounds()
{
    std::vector<std::string> sounds;
    sounds.swap(sounds_);
    return sounds;
}

bool CitySession::take_budget_request()
{
    const bool requested = budget_requested_;
    budget_requested_ = false;
    return requested;
}

void CitySession::place_sprite(int type, int tile_x, int tile_y)
{
    if (!ready_) {
        return;
    }
    if (type <= SPRITE_NOTUSED || type >= SPRITE_COUNT) {
        return;
    }
    engine_->sim.makeSprite(type, tile_x << 4, tile_y << 4);
}

bool CitySession::stamp_neighborhood(int &origin_x, int &origin_y)
{
    origin_x = 0;
    origin_y = 0;
    if (!ready_) {
        return false;
    }
    Micropolis &sim = engine_->sim;
    constexpr int kWide = 20;
    constexpr int kTall = 14;
    auto open_land = [](int raw) {
        const int tile = raw & LOMASK;
        return tile == DIRT || (tile >= TREEBASE && tile <= WOODS5) ||
               (tile >= RUBBLE && tile <= LASTRUBBLE);
    };
    bool found = false;
    for (int y = 2; y < kWorldH - kTall && !found; ++y) {
        for (int x = 2; x < kWorldW - kWide; ++x) {
            bool clear = true;
            for (int dy = 0; dy < kTall && clear; ++dy) {
                for (int dx = 0; dx < kWide; ++dx) {
                    if (!open_land(sim.map[x + dx][y + dy])) {
                        clear = false;
                        break;
                    }
                }
            }
            if (clear) {
                origin_x = x;
                origin_y = y;
                found = true;
            }
        }
    }
    if (!found) {
        return false;
    }
    for (int dy = 0; dy < kTall; ++dy) {
        for (int dx = 0; dx < kWide; ++dx) {
            if ((sim.map[origin_x + dx][origin_y + dy] & LOMASK) != DIRT) {
                sim.doTool(TOOL_BULLDOZER, static_cast<short>(origin_x + dx),
                           static_cast<short>(origin_y + dy));
            }
        }
    }

    const int ox = origin_x;
    const int oy = origin_y;
    auto put = [&](EditingTool tool, int x, int y) {
        sim.doTool(tool, static_cast<short>(x), static_cast<short>(y));
    };

    // Click is the building center. 4x4 coal occupies center-1 .. center+2.
    put(TOOL_COALPOWER, static_cast<short>(ox + 3), static_cast<short>(oy + 3));
    put(TOOL_RESIDENTIAL, static_cast<short>(ox + 9), static_cast<short>(oy + 3));
    put(TOOL_COMMERCIAL, static_cast<short>(ox + 15), static_cast<short>(oy + 3));
    put(TOOL_INDUSTRIAL, static_cast<short>(ox + 3), static_cast<short>(oy + 9));
    put(TOOL_FIRESTATION, static_cast<short>(ox + 9), static_cast<short>(oy + 9));
    put(TOOL_POLICESTATION, static_cast<short>(ox + 15), static_cast<short>(oy + 9));
    put(TOOL_PARK, static_cast<short>(ox + 13), static_cast<short>(oy + 8));

    for (int y = oy; y < oy + kTall; ++y) {
        for (int x = ox; x < ox + kWide; ++x) {
            const bool grid = (x - ox) % 6 == 0 || (y - oy) % 6 == 0;
            if (grid && (sim.map[x][y] & LOMASK) == DIRT) {
                put(TOOL_ROAD, x, y);
            }
        }
    }
    for (int x = ox + 1; x < ox + kWide - 1; ++x) {
        put(TOOL_WIRE, x, oy + 6);
    }
    notify();
    return true;
}

void CitySession::disaster_fire()
{
    engine_->sim.makeFire();
    notify();
}

void CitySession::disaster_flood()
{
    engine_->sim.makeFlood();
    notify();
}

void CitySession::disaster_tornado()
{
    engine_->sim.makeTornado();
    notify();
}

void CitySession::disaster_earthquake()
{
    engine_->sim.makeEarthquake();
    notify();
}

void CitySession::disaster_monster()
{
    engine_->sim.makeMonster();
    notify();
}

void CitySession::disaster_meltdown()
{
    engine_->sim.makeMeltdown();
    notify();
}

std::string CitySession::city_name() const
{
    if (engine_->sim.cityName.empty()) {
        return "New City";
    }
    return engine_->sim.cityName;
}

std::string CitySession::funds_text() const
{
    return "Funds: " + with_commas(static_cast<long>(engine_->sim.totalFunds));
}

std::string CitySession::date_text() const
{
    int month = static_cast<int>(engine_->sim.cityMonth);
    if (month < 0 || month > 11) {
        month = 0;
    }
    const long year = static_cast<long>(engine_->sim.cityYear);
    return std::string(kMonths[month]) + " " + std::to_string(year > 0 ? year : engine_->sim.startingYear);
}

std::string CitySession::message() const
{
    return message_;
}

std::string CitySession::evaluation_text()
{
    Micropolis &sim = engine_->sim;
    sim.cityEvaluation();
    const long population =
        (static_cast<long>(sim.resPop) + (static_cast<long>(sim.comPop) + sim.indPop) * 8L) * 20L;
    std::ostringstream out;
    out << city_class_name(sim.cityClass) << "\n"
        << "Population: " << population << "\n"
        << "Residential / Commercial / Industrial: " << sim.resPop << " / " << sim.comPop
        << " / " << sim.indPop << "\n"
        << "Score: " << sim.cityScore << "\n"
        << "Tax: " << sim.cityTax << "%";
    return out.str();
}

std::string CitySession::budget_text() const
{
    const BudgetBook book = budget();
    std::ostringstream out;
    out << "Road funding: " << book.road_percent << "%\n"
        << "Police funding: " << book.police_percent << "%\n"
        << "Fire funding: " << book.fire_percent << "%\n"
        << (engine_->sim.autoBudget ? "Auto budget is on." : "Auto budget is off.");
    return out.str();
}

namespace {

double clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0;
    }
    if (value > 1.0f) {
        return 1.0;
    }
    return value;
}

} // namespace

double CitySession::res_demand()
{
    float residential = 0;
    float commercial = 0;
    float industrial = 0;
    engine_->sim.getDemands(&residential, &commercial, &industrial);
    return clamp_unit(residential);
}

double CitySession::com_demand()
{
    float residential = 0;
    float commercial = 0;
    float industrial = 0;
    engine_->sim.getDemands(&residential, &commercial, &industrial);
    return clamp_unit(commercial);
}

double CitySession::ind_demand()
{
    float residential = 0;
    float commercial = 0;
    float industrial = 0;
    engine_->sim.getDemands(&residential, &commercial, &industrial);
    return clamp_unit(industrial);
}

int CitySession::map_value(int x, int y) const
{
    if (!ready_ || !Micropolis::testBounds(x, y) || engine_->sim.map[x] == nullptr) {
        return 0;
    }
    return engine_->sim.map[x][y];
}

int CitySession::layer_value(MapLayer layer, int x, int y) const
{
    if (!ready_ || !Micropolis::testBounds(x, y)) {
        return 0;
    }
    Micropolis &sim = engine_->sim;
    const int raw = sim.map[x][y];
    const int tile = raw & LOMASK;
    const bool water = (tile >= RIVER && tile <= WATER_HIGH) || (tile >= FLOOD && tile <= LASTFLOOD);
    switch (layer) {
    case MapLayer::Water:
        return water ? 1 : 0;
    case MapLayer::Power:
        if (water || tile <= LASTFIRE) {
            return 0;
        }
        if ((raw & ZONEBIT) != 0) {
            return (raw & PWRBIT) != 0 ? 3 : 2;
        }
        if ((raw & CONDBIT) != 0) {
            return 4;
        }
        return 0;
    case MapLayer::Pollution:
        return sim.getPollutionDensity(x / 2, y / 2);
    case MapLayer::Crime:
        return sim.getCrimeRate(x / 2, y / 2);
    case MapLayer::LandValue:
        return sim.getLandValue(x / 2, y / 2);
    case MapLayer::Traffic:
        return sim.getTrafficDensity(x / 2, y / 2);
    }
    return 0;
}

unsigned CitySession::map_serial() const
{
    return static_cast<unsigned>(engine_->sim.mapSerial);
}

std::vector<CitySession::SpriteDot> CitySession::sprites() const
{
    std::vector<SpriteDot> dots;
    if (!ready_) {
        return dots;
    }
    int guard = 0;
    for (SimSprite *sprite = engine_->sim.spriteList; sprite != nullptr && guard < 64;
         sprite = sprite->next, ++guard) {
        if (sprite->frame == 0 || sprite->type == SPRITE_NOTUSED) {
            continue;
        }
        SpriteDot dot;
        dot.type = sprite->type;
        dot.frame = sprite->frame;
        dot.x = sprite->x;
        dot.y = sprite->y;
        dot.x_offset = sprite->xOffset;
        dot.y_offset = sprite->yOffset;
        dot.width = sprite->width;
        dot.height = sprite->height;
        dot.tile_x = sprite->x >> 4;
        dot.tile_y = sprite->y >> 4;
        dots.push_back(dot);
    }
    return dots;
}

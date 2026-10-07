// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "city_session.hpp"

#include "assets.hpp"
#include "messages.hpp"

#include "micropolis.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
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

Quad live_population_count(const Micropolis &sim)
{
    // Phase 0 clears the live counts. The snapshot is the last finished scan.
    const short res = sim.censusSnapshotValid ? sim.snapResPop : sim.resPop;
    const short com = sim.censusSnapshotValid ? sim.snapComPop : sim.comPop;
    const short ind = sim.censusSnapshotValid ? sim.snapIndPop : sim.indPop;
    return (static_cast<Quad>(res) + (static_cast<Quad>(com) + static_cast<Quad>(ind)) * 8L) * 20L;
}

CityClass live_city_class(Quad population)
{
    CityClass city_class = CC_VILLAGE;
    if (population > 2000) {
        city_class = CC_TOWN;
    }
    if (population > 10000) {
        city_class = CC_CITY;
    }
    if (population > 50000) {
        city_class = CC_CAPITAL;
    }
    if (population > 100000) {
        city_class = CC_METROPOLIS;
    }
    if (population > 500000) {
        city_class = CC_MEGALOPOLIS;
    }
    return city_class;
}

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

struct ScenarioEntry {
    CitySession::ScenarioDef def;
    const char *filename;
};

// Names, years, funds, and filenames are the engine's loadScenario() table.
const ScenarioEntry kScenarios[] = {
    {{SC_DULLSVILLE, "Dullsville", 1900, "A quiet city that needs growth."}, "snro.111"},
    {{SC_SAN_FRANCISCO, "San Francisco", 1906, "An earthquake strikes the city."}, "snro.222"},
    {{SC_HAMBURG, "Hamburg", 1944, "Fire spreads through the city."}, "snro.333"},
    {{SC_BERN, "Bern", 1965, "Traffic is choking the streets."}, "snro.444"},
    {{SC_TOKYO, "Tokyo", 1957, "A monster is approaching."}, "snro.555"},
    {{SC_DETROIT, "Detroit", 1972, "Crime is out of control."}, "snro.666"},
    {{SC_BOSTON, "Boston", 2010, "A nuclear meltdown is coming."}, "snro.777"},
    {{SC_RIO, "Rio de Janeiro", 2047, "Flooding threatens the city."}, "snro.888"},
};

static_assert(sizeof(kScenarios) / sizeof(kScenarios[0]) == CitySession::kScenarioCount,
              "scenario catalog size");
static_assert(SC_DULLSVILLE == 1 && SC_RIO == 8, "scenario ids");
static_assert(static_cast<int>(LEVEL_EASY) == CitySession::kLevelEasy, "easy level");
static_assert(static_cast<int>(LEVEL_MEDIUM) == CitySession::kLevelMedium, "medium level");
static_assert(static_cast<int>(LEVEL_HARD) == CitySession::kLevelHard, "hard level");
static_assert(static_cast<int>(HISTORY_TYPE_RES) == static_cast<int>(CitySession::HistorySeries::Residential),
              "residential history");
static_assert(static_cast<int>(HISTORY_TYPE_COM) == static_cast<int>(CitySession::HistorySeries::Commercial),
              "commercial history");
static_assert(static_cast<int>(HISTORY_TYPE_IND) == static_cast<int>(CitySession::HistorySeries::Industrial),
              "industrial history");
static_assert(static_cast<int>(HISTORY_TYPE_MONEY) == static_cast<int>(CitySession::HistorySeries::CashFlow),
              "cash-flow history");
static_assert(static_cast<int>(HISTORY_TYPE_CRIME) == static_cast<int>(CitySession::HistorySeries::Crime),
              "crime history");
static_assert(static_cast<int>(HISTORY_TYPE_POLLUTION) == static_cast<int>(CitySession::HistorySeries::Pollution),
              "pollution history");
static_assert(static_cast<int>(HISTORY_SCALE_SHORT) == static_cast<int>(CitySession::HistoryScale::Short),
              "short history");
static_assert(static_cast<int>(HISTORY_SCALE_LONG) == static_cast<int>(CitySession::HistoryScale::Long),
              "long history");
static_assert(HISTORY_COUNT == CitySession::kHistoryPoints, "history length");
static_assert(CVP_CRIME == 0 && CVP_FIRE == 6 && CVP_NUMPROBLEMS == 7, "problem ids");

} // namespace

const CitySession::ScenarioDef &CitySession::scenario_def(int index)
{
    if (index < 0 || index >= kScenarioCount) {
        return kScenarios[0].def;
    }
    return kScenarios[index].def;
}

// Held out of the header so the UI translation units do not include the engine.
struct CitySession::Engine {
    Micropolis sim;
};

int hostile_review_session_probe(CitySession &session, int op)
{
    Micropolis &sim = session.engine_->sim;
    if (op == 1) {
        sim.budgetAwaitingAccept = true;
        return 0;
    }
    if (op == 2) {
        // A finished census, and the next simulator step is phase 0.
        sim.resPop = 40;
        sim.comPop = 2;
        sim.indPop = 1;
        sim.censusSnapshotValid = false;
        sim.liveCensusComplete = true;
        sim.phaseCycle = 0;
        sim.setSpeed(3);
        return 0;
    }
    sim.cityAssessedValue = 424242;
    const bool ran = session.note_evaluation_month(session.game_month_index() + 50);
    if (ran || sim.cityAssessedValue != 424242) {
        std::fprintf(stderr, "ran %d assessed %ld\n", ran ? 1 : 0, static_cast<long>(sim.cityAssessedValue));
        return 1;
    }
    return 0;
}

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
    struct CallbackGuard {
        bool &flag;
        bool outer;
        explicit CallbackGuard(bool &flag) : flag(flag), outer(!flag) { flag = true; }
        ~CallbackGuard()
        {
            if (outer) {
                flag = false;
            }
        }
    } guard(in_callback_);

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
        ++query_serial_;
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

    if (which == "autoGoto") {
        int x = 0;
        int y = 0;
        const char *p = params != nullptr ? params : "";
        if (*p == 'd') {
            x = take_int();
            ++p;
        }
        if (*p == 'd') {
            y = take_int();
        }
        if (Micropolis::testBounds(x, y)) {
            goto_x_ = x;
            goto_y_ = y;
            goto_pending_ = true;
            notify();
        }
        return;
    }

    if (which == "startEarthquake") {
        quake_strength_ = (params != nullptr && params[0] == 'd') ? take_int() : 0;
        if (quake_strength_ < 1) {
            quake_strength_ = 1;
        }
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
    NewCitySpec spec;
    spec.name = name;
    spec.seed = seed;
    // The historical call treats 0 as "choose from the clock".
    spec.seed_was_set = seed != 0;
    new_city(spec);
}

void CitySession::new_city(const NewCitySpec &spec)
{
    ready_ = false;
    Micropolis &sim = engine_->sim;
    // A budget window left open must not charge this city when it closes.
    sim.budgetAwaitingAccept = false;
    // Micropolis::init() (called from the constructor) already ran simInit().
    // simInit() is private and reallocates history buffers, so a new city
    // sets the terrain knobs, lets generateSomeCity() rebuild the map, then
    // applies the difficulty funds. generateMap() reads these fields directly.
    sim.terrainCreateIsland = spec.island;
    sim.terrainCurveLevel = spec.rivers;
    sim.terrainLakeLevel = spec.lakes;
    sim.terrainTreeLevel = spec.trees;
    sim.setCityTax(7);
    sim.setAutoBudget(true);
    sim.setAutoBulldoze(true);
    sim.setEnableDisasters(true);
    const std::string city = spec.name.empty() ? "New City" : spec.name;
    sim.setCleanCityName(city);
    sim.setSpeed(static_cast<short>(speed_));
    sim.setPasses(1);
    // An explicit seed, including 0, is passed through. Clock/auto is only
    // the unset case (seed 0 and seed_was_set false).
    const int used_seed = (spec.seed == 0 && !spec.seed_was_set)
                              ? static_cast<int>(std::time(nullptr))
                              : spec.seed;
    sim.generateSomeCity(used_seed);
    int level = spec.difficulty;
    if (level < kLevelEasy || level > kLevelHard) {
        level = kLevelEasy;
    }
    sim.setGameLevelFunds(static_cast<GameLevel>(level));
    sim.setSpeed(static_cast<short>(speed_));
    sim.setEnableSound(sound_enabled_);
    // A new city follows disasters. A loaded city keeps the flag in the file.
    sim.setAutoGoto(true);
    // The last city's road, police, and fire rates must not bill this map.
    sim.budgetAwaitingAccept = false;
    sim.initFundingLevel();
    sim.updateFundEffects();
    sim.evalPreviewValid = false;
    save_path_.clear();
    message_.clear();
    dirty_ = false;
    eval_month_ = -1;
    eval_month_pending_ = false;
    ready_ = true;
    notify();
}

void CitySession::rename_city(const std::string &name)
{
    std::string clean;
    clean.reserve(name.size());
    bool pending_space = false;
    for (unsigned char ch : name) {
        if (ch == ' ' || ch == '\t') {
            if (!clean.empty()) {
                pending_space = true;
            }
            continue;
        }
        if (ch < 32) {
            continue;
        }
        if (pending_space) {
            clean.push_back(' ');
            pending_space = false;
        }
        clean.push_back(static_cast<char>(ch));
        if (clean.size() >= 48) {
            break;
        }
    }
    if (clean.empty()) {
        return;
    }
    const std::string before = engine_->sim.cityName;
    engine_->sim.setCleanCityName(clean);
    if (engine_->sim.cityName == before) {
        return;
    }
    dirty_ = true;
    notify();
}

int CitySession::difficulty() const
{
    return static_cast<int>(engine_->sim.gameLevel);
}

long CitySession::funds() const
{
    return static_cast<long>(engine_->sim.totalFunds);
}

int CitySession::generated_seed() const
{
    return engine_->sim.generatedCitySeed;
}

bool CitySession::load_city(const std::string &path)
{
    message_.clear();
    Micropolis &sim = engine_->sim;
    if (!sim.loadCity(path.c_str())) {
        message_ = "Could not load that city file.";
        notify();
        return false;
    }
    sim.setSpeed(static_cast<short>(speed_));
    sound_enabled_ = sim.enableSound;
    sim.budgetAwaitingAccept = false;
    sim.evalPreviewValid = false;
    save_path_ = path;
    ready_ = true;
    dirty_ = false;
    eval_month_ = -1;
    eval_month_pending_ = false;
    message_ = "Loaded a saved city.";
    notify();
    return true;
}

bool CitySession::load_scenario(int id)
{
    const ScenarioEntry *entry = nullptr;
    for (const auto &candidate : kScenarios) {
        if (candidate.def.id == id) {
            entry = &candidate;
            break;
        }
    }
    if (entry == nullptr) {
        message_ = "Could not start that scenario.";
        notify();
        return false;
    }

    const std::string root = asset_root();
    const std::string dir = root.empty() ? std::string() : root + "/res";
    const std::string file = dir.empty() ? std::string() : dir + "/" + entry->filename;
    std::ifstream in(file, std::ios::binary);
    in.seekg(0, std::ios::end);
    const auto bytes = in.good() ? static_cast<long>(in.tellg()) : -1L;
    if (bytes != 27120) {
        message_ = "Could not start that scenario.";
        notify();
        return false;
    }

    Micropolis &sim = engine_->sim;
    // loadScenario() reads snro.* from resourceDir via loadFileDir().
    // A failed second open must keep the city that is already loaded.
    const std::string previous_name = sim.cityName;
    sim.resourceDir = dir;
    if (!sim.loadScenario(static_cast<Scenario>(id))) {
        message_ = "Could not start that scenario.";
        notify();
        return false;
    }
    if (sim.cityName.empty()) {
        sim.setCleanCityName(previous_name);
    }
    sim.budgetAwaitingAccept = false;
    sim.evalPreviewValid = false;
    sim.setSpeed(static_cast<short>(speed_));
    sim.setEnableSound(sound_enabled_);
    save_path_.clear();
    ready_ = true;
    dirty_ = false;
    eval_month_ = -1;
    eval_month_pending_ = false;
    message_ = std::string("Playing ") + entry->def.name + ".";
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
    dirty_ = false;
    message_ = "City saved.";
    notify();
    return true;
}

void CitySession::tick()
{
    if (!ready_ || simulation_paused_) {
        return;
    }
    // simTick() steps the simulation and does not cycle animated map
    // tiles. Traffic, fountains, and smokestacks advance on the same
    // tick while the city is running. Pause (speed 0) leaves them still.
    engine_->sim.simTick();
    if (speed_ != 0) {
        engine_->sim.animateTiles();
    }
}

void CitySession::use_tool(int engine_tool, int tile_x, int tile_y)
{
    if (!ready_) {
        return;
    }
    engine_->sim.toolDown(static_cast<EditingTool>(engine_tool),
                          static_cast<short>(tile_x), static_cast<short>(tile_y));
    if (engine_tool != TOOL_QUERY) {
        dirty_ = true;
    }
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
    if (engine_tool != TOOL_QUERY) {
        dirty_ = true;
    }
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

void CitySession::set_simulation_paused(bool paused)
{
    simulation_paused_ = paused;
}

bool CitySession::simulation_paused() const
{
    return simulation_paused_;
}

int CitySession::game_month_index() const
{
    const Micropolis &sim = engine_->sim;
    int year = static_cast<int>(sim.cityYear);
    if (year <= 0) {
        year = static_cast<int>(sim.startingYear);
    }
    int month = static_cast<int>(sim.cityMonth);
    if (month < 0) {
        month = 0;
    }
    if (month > 11) {
        month = 11;
    }
    return year * 12 + month;
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

void CitySession::set_auto_goto(bool on)
{
    engine_->sim.setAutoGoto(on);
}

bool CitySession::auto_goto() const
{
    return engine_->sim.autoGoto;
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
    float *slot = &sim.roadPercent;
    if (kind == 1) {
        slot = &sim.policePercent;
    } else if (kind == 2) {
        slot = &sim.firePercent;
    }
    // Effects wait until the budget window commits. Moving a slider used
    // to drop road and coverage before any money moved.
    *slot = fraction;
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
    book.previous_funds = sim.budgetAnchorValid ? static_cast<long>(sim.budgetAnchorFunds) : book.funds;
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

void CitySession::keep_budget_request()
{
    budget_requested_ = true;
}

void CitySession::commit_pending_budget()
{
    if (engine_->sim.budgetAwaitingAccept) {
        dirty_ = true;
    }
    engine_->sim.commitBudgetPayment();
}

bool CitySession::budget_pending() const
{
    return engine_->sim.budgetAwaitingAccept;
}

void CitySession::discard_pending_budget()
{
    engine_->sim.budgetAwaitingAccept = false;
}

void CitySession::finish_budget_edit()
{
    if (engine_->sim.budgetAwaitingAccept) {
        commit_pending_budget();
        return;
    }
    engine_->sim.updateFundEffects();
}

bool CitySession::census_ready() const
{
    return engine_->sim.liveCensusComplete;
}

bool CitySession::note_evaluation_month(int month_index)
{
    if (month_index != eval_month_) {
        eval_month_pending_ = true;
    }
    // Phase 0 publishes the new month and clears the census in the same
    // tick. Wait until the scan has filled the counts again.
    if (in_callback_ || !census_ready()) {
        return false;
    }
    if (!eval_month_pending_) {
        return false;
    }
    eval_month_ = month_index;
    eval_month_pending_ = false;
    engine_->sim.cityEvaluationPreview();
    return true;
}

long CitySession::stored_assessed_value() const
{
    return static_cast<long>(engine_->sim.cityAssessedValue);
}

bool CitySession::needs_save_prompt() const
{
    return dirty_ || budget_pending();
}

bool CitySession::take_view_target(int &tile_x, int &tile_y)
{
    if (!goto_pending_) {
        return false;
    }
    goto_pending_ = false;
    tile_x = goto_x_;
    tile_y = goto_y_;
    return true;
}

int CitySession::take_earthquake()
{
    const int strength = quake_strength_;
    quake_strength_ = 0;
    return strength;
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
    dirty_ = true;
    notify();
}

void CitySession::disaster_flood()
{
    engine_->sim.makeFlood();
    dirty_ = true;
    notify();
}

void CitySession::disaster_tornado()
{
    engine_->sim.makeTornado();
    dirty_ = true;
    notify();
}

void CitySession::disaster_earthquake()
{
    engine_->sim.makeEarthquake();
    dirty_ = true;
    notify();
}

void CitySession::disaster_monster()
{
    engine_->sim.makeMonster();
    dirty_ = true;
    notify();
}

void CitySession::disaster_meltdown()
{
    engine_->sim.makeMeltdown();
    dirty_ = true;
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
    update_evaluation();
    const Evaluation report = evaluation();
    std::ostringstream out;
    out << report.category << "\n"
        << "Population: " << report.population << "\n"
        << "Score: " << report.score << "\n"
        << "Yes: " << report.yes_percent << "%";
    return out.str();
}

int CitySession::history_value(HistorySeries series, HistoryScale scale, int index) const
{
    return engine_->sim.getHistory(static_cast<int>(series), static_cast<int>(scale), index);
}

void CitySession::update_evaluation()
{
    // Preview fills problems and opinion for the window. Population,
    // migration, and score stay on the tax-year pass. Skip the pass while
    // phase 0 has cleared the census, and while an engine callback is
    // still on the stack.
    if (in_callback_ || !census_ready()) {
        eval_month_pending_ = true;
        return;
    }
    engine_->sim.cityEvaluationPreview();
}

CitySession::Evaluation CitySession::evaluation() const
{
    const Micropolis &sim = engine_->sim;
    Evaluation report;
    report.score = sim.cityScore;
    report.score_delta = sim.cityScoreDelta;
    report.yes_percent = sim.evalPreviewValid ? sim.evalPreviewYes : sim.cityYes;
    if (report.yes_percent < 0) {
        report.yes_percent = 0;
    }
    if (report.yes_percent > 100) {
        report.yes_percent = 100;
    }
    const Quad live_population = live_population_count(sim);
    report.population = live_population < 0 ? 0 : static_cast<long>(live_population);
    // cityPopDelta is the last tax year. A mid-year preview does not replace it.
    report.migration = static_cast<long>(sim.cityPopDelta);
    report.assessed_value = static_cast<long>(sim.evalPreviewValid ? sim.evalPreviewAssessed : sim.cityAssessedValue);
    report.category = city_class_name(live_city_class(live_population < 0 ? 0 : live_population));
    report.year = static_cast<int>(sim.cityYear > 0 ? sim.cityYear : sim.startingYear);
    switch (sim.gameLevel) {
    case LEVEL_MEDIUM:
        report.difficulty = "Medium";
        break;
    case LEVEL_HARD:
        report.difficulty = "Hard";
        break;
    case LEVEL_EASY:
    default:
        report.difficulty = "Easy";
        break;
    }

    static const char *kProblems[] = {
        "Crime", "Pollution", "Housing", "Taxes", "Traffic", "Unemployment", "Fire",
    };
    for (int i = 0; i < CVP_PROBLEM_COMPLAINTS; ++i) {
        const int which = sim.evalPreviewValid ? sim.evalPreviewOrder[i] : sim.problemOrder[i];
        if (which < 0 || which >= CVP_NUMPROBLEMS) {
            break;
        }
        Problem problem;
        problem.name = kProblems[which];
        const int votes = sim.evalPreviewValid ? sim.evalPreviewVotes[which] : sim.problemVotes[which];
        problem.votes = votes;
        if (problem.votes < 0) {
            problem.votes = 0;
        }
        report.problems.push_back(problem);
    }
    return report;
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
    // Deactivated sprites used to sit on this list forever. Keep a cycle
    // guard, but do not stop after a handful of nodes or a later train,
    // plane, ship, or monster is never drawn.
    for (SimSprite *sprite = engine_->sim.spriteList; sprite != nullptr && guard < 4096;
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

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
    if (op == 3) {
        sim.roadFund = 1000;
        sim.roadSpend = 1000;
        sim.roadPercent = 1.0f;
        sim.fireFund = 0;
        sim.policeFund = 0;
        sim.updateFundEffects();
        return static_cast<int>(sim.roadEffect);
    }
    if (op == 4) {
        return static_cast<int>(sim.roadEffect);
    }
    if (op == 5) {
        sim.doWinGame();
        return session.speed();
    }
    if (op == 6) {
        sim.doLoseGame();
        return session.speed();
    }
    if (op == 8) {
        sim.autoBudget = false;
        sim.taxFund = 2000;
        sim.roadFund = 400;
        sim.policeFund = 0;
        sim.fireFund = 0;
        sim.roadPercent = 1.0f;
        sim.policePercent = 0.0f;
        sim.firePercent = 0.0f;
        sim.setFunds(8000);
        sim.budgetAnchorFunds = 8000;
        sim.budgetAnchorValid = true;
        sim.doBudgetNow(false);
        return sim.budgetAwaitingAccept ? static_cast<int>(sim.totalFunds) : -1;
    }
    if (op == 9) {
        sim.autoBudget = false;
        sim.taxFund = 0;
        sim.roadFund = 5000;
        sim.policeFund = 0;
        sim.fireFund = 0;
        sim.roadPercent = 1.0f;
        sim.policePercent = 0.0f;
        sim.firePercent = 0.0f;
        sim.setFunds(100);
        sim.doBudgetNow(false);
        sim.roadPercent = 1.0f;
        return sim.budgetAwaitingAccept ? 0 : 1;
    }
    if (op == 10) {
        sim.map[10][10] = RIVER;
        sim.map[9][10] = ROADS | BULLBIT | BURNBIT;
        sim.map[11][10] = ROADS | BULLBIT | BURNBIT;
        return 0;
    }
    if (op == 11) {
        sim.map[20][20] = RIVER;
        sim.map[19][20] = DIRT;
        sim.map[21][20] = DIRT;
        sim.map[20][19] = DIRT;
        sim.map[20][21] = DIRT;
        return 0;
    }
    if (op == 12) {
        // Rail on both banks, and a clear land tile for the dry-land price.
        // The other axis is dirt so the span uses the banks that were set.
        sim.map[10][12] = RIVER;
        sim.map[9][12] = HRAIL | BULLBIT | BURNBIT;
        sim.map[11][12] = HRAIL | BULLBIT | BURNBIT;
        sim.map[10][11] = DIRT;
        sim.map[10][13] = DIRT;
        sim.map[30][16] = RIVER;
        sim.map[30][15] = VRAIL | BULLBIT | BURNBIT;
        sim.map[30][17] = VRAIL | BULLBIT | BURNBIT;
        sim.map[29][16] = DIRT;
        sim.map[31][16] = DIRT;
        sim.map[5][5] = DIRT;
        return 0;
    }
    if (op == 13) {
        // Conductive tiles the wire tool will anchor to. Horizontal banks
        // must not be HPOWER; vertical banks must not be VPOWER.
        sim.map[10][14] = RIVER;
        sim.map[9][14] = LHPOWER | CONDBIT | BURNBIT | BULLBIT;
        sim.map[11][14] = LHPOWER | CONDBIT | BURNBIT | BULLBIT;
        sim.map[10][13] = DIRT;
        sim.map[10][15] = DIRT;
        sim.map[14][10] = RIVER;
        sim.map[14][9] = HPOWER | CONDBIT | BURNBIT | BULLBIT;
        sim.map[14][11] = HPOWER | CONDBIT | BURNBIT | BULLBIT;
        sim.map[13][10] = DIRT;
        sim.map[15][10] = DIRT;
        sim.map[6][6] = DIRT;
        return 0;
    }
    if (op == 14) {
        sim.autoBudget = false;
        sim.taxFund = 0;
        sim.roadFund = 5000;
        sim.policeFund = 0;
        sim.fireFund = 4000;
        sim.roadPercent = 1.0f;
        sim.policePercent = 1.0f;
        sim.firePercent = 1.0f;
        sim.setFunds(100);
        sim.doBudgetNow(false);
        return sim.budgetAwaitingAccept ? static_cast<int>(sim.totalFunds) : -1;
    }
    if (op == 15) {
        sim.budgetAwaitingAccept = false;
        sim.taxFund = 2000;
        return static_cast<int>(sim.taxFund);
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

void CitySession::post_message(std::string text)
{
    message_ = std::move(text);
    ++message_serial_;
    notify();
}

void CitySession::mark_dirty()
{
    if (ready_) {
        dirty_ = true;
    }
}

bool CitySession::name_is_usable(const std::string &name)
{
    return acceptable_city_name(name);
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
            post_message(message_for_number(number));
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

    if (which == "winGame" || which == "loseGame") {
        // The window is told after the clock is already 0. Remember the
        // speed that was running, once, so Keep playing and the next
        // scenario can put it back.
        if (!outcome_paused_) {
            speed_before_outcome_ = speed_;
        }
        outcome_paused_ = true;
        scenario_outcome_ = which == "winGame" ? 1 : -1;
        speed_ = 0;
        engine_->sim.setSpeed(0);
        mark_dirty();
        notify();
        return;
    }

    if (which == "didntLoadCity" || which == "didntSaveCity") {
        const char *msg = (params != nullptr && params[0] == 's') ? take_string() : "";
        std::string text = std::string(which == "didntLoadCity" ? "Could not load " : "Could not save ") + msg;
        if (which == "didntSaveCity" && !engine_->sim.saveErrorDetail.empty()) {
            text += ": ";
            text += engine_->sim.saveErrorDetail;
        }
        post_message(std::move(text));
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
    if (!spec.name.empty() && !name_is_usable(spec.name)) {
        post_message("Enter a name that is not only spaces.");
        return;
    }
    outcome_paused_ = false;
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
    // generateSomeCity() already ran setValves at the previous city's
    // level. Store this city's level before the next even sim cycle.
    if (sim.miscHist != nullptr && sim.gameLevel >= LEVEL_FIRST && sim.gameLevel <= LEVEL_LAST) {
        sim.miscHist[15] = static_cast<short>(sim.gameLevel);
    }
    sim.setSpeed(static_cast<short>(speed_));
    sim.setEnableSound(sound_enabled_);
    // A new city starts with disasters on. A loaded city reads miscHist[64]:
    // 1 is on, 2 is off, and every other value (including 0) stays on.
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
        post_message("Enter a name that is not only spaces.");
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
    int loaded_speed = static_cast<int>(sim.simSpeed);
    if (loaded_speed < 0) {
        loaded_speed = 0;
    }
    if (loaded_speed > 3) {
        loaded_speed = 3;
    }
    outcome_paused_ = false;
    speed_ = loaded_speed;
    sound_enabled_ = sim.enableSound;
    sim.budgetAwaitingAccept = false;
    sim.evalPreviewValid = false;
    save_path_ = path;
    ready_ = true;
    dirty_ = false;
    eval_month_ = -1;
    eval_month_pending_ = false;
    post_message("Loaded a saved city.");
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
    // loadScenario() starts at Fast. A win or loss has paused this
    // session for the announcement; the next city runs at the speed
    // from before that pause. Any other session speed is kept.
    if (outcome_paused_) {
        speed_ = std::max(0, std::min(3, speed_before_outcome_));
        outcome_paused_ = false;
    }
    sim.setSpeed(static_cast<short>(speed_));
    sim.setEnableSound(sound_enabled_);
    save_path_.clear();
    ready_ = true;
    dirty_ = false;
    eval_month_ = -1;
    eval_month_pending_ = false;
    post_message(std::string("Playing ") + entry->def.name + ".");
    return true;
}

bool CitySession::save_city_as(const std::string &path)
{
    message_.clear();
    // saveCityAs reports failure through the callback hook, including the
    // operating-system reason captured on the failing call.
    if (!engine_->sim.saveCityAs(path.c_str())) {
        if (message_.rfind("Could not save", 0) != 0) {
            std::string text = "Could not save " + path;
            if (!engine_->sim.saveErrorDetail.empty()) {
                text += ": " + engine_->sim.saveErrorDetail;
            }
            post_message(std::move(text));
        }
        return false;
    }
    save_path_ = path;
    dirty_ = false;
    post_message("City saved.");
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
    const ToolResult result = engine_->sim.toolDown(static_cast<EditingTool>(engine_tool),
                                                    static_cast<short>(tile_x), static_cast<short>(tile_y));
    if (result == TOOLRESULT_OK && engine_tool != TOOL_QUERY) {
        dirty_ = true;
    }
    notify();
}

void CitySession::drag_tool(int engine_tool, int from_x, int from_y, int to_x, int to_y)
{
    if (!ready_) {
        return;
    }
    const bool placed = engine_->sim.toolDrag(static_cast<EditingTool>(engine_tool),
                                              static_cast<short>(from_x), static_cast<short>(from_y),
                                              static_cast<short>(to_x), static_cast<short>(to_y));
    if (placed) {
        dirty_ = true;
    }
    notify();
}

void CitySession::set_speed(int speed)
{
    speed = std::max(0, std::min(3, speed));
    // A running speed is the player's choice. Pause leaves an announcement
    // pause in place so the next scenario can still restore the old speed.
    if (speed > 0) {
        outcome_paused_ = false;
    }
    if (speed_ == speed && (!ready_ || engine_->sim.simSpeed == speed)) {
        return;
    }
    speed_ = speed;
    if (ready_) {
        engine_->sim.setSpeed(static_cast<short>(speed_));
        dirty_ = true;
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
    if (engine_->sim.autoBudget == on) {
        return;
    }
    engine_->sim.setAutoBudget(on);
    mark_dirty();
}

bool CitySession::auto_budget() const
{
    return engine_->sim.autoBudget;
}

void CitySession::set_auto_bulldoze(bool on)
{
    if (engine_->sim.autoBulldoze == on) {
        return;
    }
    engine_->sim.setAutoBulldoze(on);
    mark_dirty();
}

bool CitySession::auto_bulldoze() const
{
    return engine_->sim.autoBulldoze;
}

void CitySession::set_disasters(bool on)
{
    if (engine_->sim.enableDisasters == on) {
        return;
    }
    engine_->sim.setEnableDisasters(on);
    mark_dirty();
}

bool CitySession::disasters() const
{
    return engine_->sim.enableDisasters;
}

void CitySession::set_auto_goto(bool on)
{
    if (engine_->sim.autoGoto == on) {
        return;
    }
    engine_->sim.setAutoGoto(on);
    mark_dirty();
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
    if (engine_->sim.cityTax == percent) {
        return;
    }
    engine_->sim.setCityTax(static_cast<short>(percent));
    if (engine_->sim.budgetAwaitingAccept) {
        engine_->sim.recomputeTaxFund();
    }
    mark_dirty();
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
    // The player named this rate, even when doBudgetNow had already scaled
    // the department to the same number. That request is what the cut
    // sentence compares, so a slider left on 0% is not "cut to 0%".
    if (kind == 1) {
        sim.policeFundingTouched = true;
    } else if (kind == 2) {
        sim.fireFundingTouched = true;
    } else {
        sim.roadFundingTouched = true;
    }
    if (*slot == fraction) {
        return;
    }
    // Effects wait until the budget window commits. Moving a slider used
    // to drop road and coverage before any money moved.
    *slot = fraction;
    mark_dirty();
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

void CitySession::restore_budget_rates(int tax_percent, int road_percent, int police_percent, int fire_percent)
{
    set_tax(tax_percent);
    set_road_funding(road_percent);
    set_police_funding(police_percent);
    set_fire_funding(fire_percent);
    // Restoring the opening rates is not the player asking for a new share.
    engine_->sim.roadFundingTouched = false;
    engine_->sim.policeFundingTouched = false;
    engine_->sim.fireFundingTouched = false;
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
    book.tax_percent = sim.cityTax;
    book.road_percent = percent_of(sim.roadPercent);
    book.police_percent = percent_of(sim.policePercent);
    book.fire_percent = percent_of(sim.firePercent);
    book.road_need = static_cast<long>(sim.roadFund);
    book.police_need = static_cast<long>(sim.policeFund);
    book.fire_need = static_cast<long>(sim.fireFund);

    if (sim.budgetAwaitingAccept) {
        const Micropolis::BudgetCharge charge = sim.budgetCharge();
        book.road_spent = static_cast<long>(charge.roadTaken);
        book.police_spent = static_cast<long>(charge.policeTaken);
        book.fire_spent = static_cast<long>(charge.fireTaken);
        book.cash_flow = static_cast<long>(charge.posted);
        book.previous_funds = sim.budgetAnchorValid ? static_cast<long>(sim.budgetAnchorFunds)
                                                    : static_cast<long>(sim.totalFunds);
        book.funds = static_cast<long>(sim.totalFunds) + book.cash_flow;
        auto cut = [&](float requested, bool touched, float slider, float applied, Quad fund) -> std::string {
            if (fund <= 0) {
                return {};
            }
            // An untouched slider still shows the affordable share. The
            // sentence uses the request from before that scaling. After
            // the player moves the slider, that position is the request.
            const int want = percent_of(touched ? slider : requested);
            const int got = percent_of(applied);
            if (want <= got) {
                return {};
            }
            return "cut to " + std::to_string(got) + "% because the city has " +
                   with_commas(static_cast<long>(sim.totalFunds));
        };
        book.road_note = cut(sim.roadPercentRequested, sim.roadFundingTouched, sim.roadPercent,
                             charge.roadPercent, sim.roadFund);
        book.police_note = cut(sim.policePercentRequested, sim.policeFundingTouched, sim.policePercent,
                               charge.policePercent, sim.policeFund);
        book.fire_note = cut(sim.firePercentRequested, sim.fireFundingTouched, sim.firePercent,
                             charge.firePercent, sim.fireFund);
        return book;
    }

    book.funds = static_cast<long>(sim.totalFunds);
    book.previous_funds = book.funds;
    book.road_spent = funded(sim.roadFund, book.road_percent);
    book.police_spent = funded(sim.policeFund, book.police_percent);
    book.fire_spent = funded(sim.fireFund, book.fire_percent);
    book.cash_flow = book.taxes - book.road_spent - book.police_spent - book.fire_spent;
    return book;
}

void CitySession::set_sound_enabled(bool on)
{
    if (sound_enabled_ == on && engine_->sim.enableSound == on) {
        return;
    }
    sound_enabled_ = on;
    engine_->sim.setEnableSound(on);
    mark_dirty();
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
    engine_->sim.applyFundingLevels();
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

long CitySession::cash_flow_history(HistoryScale scale, int index) const
{
    return static_cast<long>(engine_->sim.cashFlowHistory(static_cast<int>(scale), index));
}

bool CitySession::cash_flow_history_exact(HistoryScale scale, int index) const
{
    return engine_->sim.cashFlowHistoryExact(static_cast<int>(scale), index);
}

int CitySession::take_scenario_outcome()
{
    const int outcome = scenario_outcome_;
    scenario_outcome_ = 0;
    return outcome;
}

void CitySession::resume_after_outcome()
{
    if (!outcome_paused_) {
        return;
    }
    const int resume = speed_before_outcome_;
    outcome_paused_ = false;
    set_speed(resume);
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

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless regressions for the round-3 review (findings 1-14 and 17).
// Layout and menu letters are covered by ui_round3.

#include "city_session.hpp"
#include "graph_legend.hpp"
#include "notice_bar.hpp"
#include "tools.hpp"

#include "micropolis.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <csignal>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

std::vector<char> read_bytes(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

short disasters_slot(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    const std::size_t offset = 6u * 240u * 2u + 64u * 2u;
    in.seekg(static_cast<std::streamoff>(offset));
    unsigned char bytes[2] = {0, 0};
    in.read(reinterpret_cast<char *>(bytes), 2);
    if (!in) {
        return -1;
    }
    return static_cast<short>((bytes[0] << 8) | bytes[1]);
}

const ToolDef *road_tool()
{
    for (int i = 0; i < kToolCount; ++i) {
        if (tool_by_index(i)->engine_id == TOOL_ROAD) {
            return tool_by_index(i);
        }
    }
    return nullptr;
}

} // namespace

int main()
{
    const std::string path = "/tmp/lunduke-round3.cty";
    std::remove(path.c_str());

    Micropolis saved;
    saved.generateSomeCity(4);
    saved.setCleanCityName("Round Three");
    saved.setFunds(5000);
    if (!saved.saveCityAs(path.c_str())) {
        return fail(1, "could not write the city used to fill the disk");
    }
    const std::vector<char> previous = read_bytes(path);
    saved.autoBudget = false;
    saved.taxFund = 2000;
    saved.roadFund = 500;
    saved.policeFund = 0;
    saved.fireFund = 0;
    saved.roadPercent = 1.0f;
    saved.policePercent = 0.0f;
    saved.firePercent = 0.0f;
    saved.setFunds(5000);
    saved.doBudgetNow(false);
    if (!saved.budgetAwaitingAccept || static_cast<long>(saved.totalFunds) != 5000) {
        return fail(2, "the tax year was not waiting before the failed save");
    }
    const float road_before = saved.roadPercent;
    std::signal(SIGXFSZ, SIG_IGN);
    struct rlimit old_limit;
    if (getrlimit(RLIMIT_FSIZE, &old_limit) != 0) {
        return fail(3, "could not read the file-size limit");
    }
    struct rlimit tight = old_limit;
    tight.rlim_cur = 1024;
    if (setrlimit(RLIMIT_FSIZE, &tight) != 0) {
        return fail(4, "could not shrink the file-size limit");
    }
    const bool wrote = saved.saveFile(path.c_str());
    const int save_errno = saved.saveErrno;
    const std::string save_detail = saved.saveErrorDetail;
    setrlimit(RLIMIT_FSIZE, &old_limit);
    if (wrote || saved.budgetAwaitingAccept == false || static_cast<long>(saved.totalFunds) != 5000 ||
        saved.roadPercent != road_before || read_bytes(path) != previous) {
        std::fprintf(stderr, "wrote %d funds %ld awaiting %d detail %s\n", wrote ? 1 : 0,
                     static_cast<long>(saved.totalFunds), saved.budgetAwaitingAccept ? 1 : 0,
                     save_detail.c_str());
        return fail(5, "a failed save collected the open tax year");
    }
    if (save_detail.empty() || save_errno == 0) {
        return fail(6, "a failed save did not keep the operating-system error");
    }
    if (!saved.saveFile(path.c_str()) || saved.budgetAwaitingAccept ||
        static_cast<long>(saved.totalFunds) != 6500) {
        std::fprintf(stderr, "funds %ld awaiting %d\n", static_cast<long>(saved.totalFunds),
                     saved.budgetAwaitingAccept ? 1 : 0);
        return fail(7, "a later successful save did not collect the same tax year");
    }

    Micropolis quiet;
    quiet.generateSomeCity(5);
    quiet.setEnableDisasters(false);
    quiet.setSpeed(0);
    const std::string flag_path = "/tmp/lunduke-round3-flags.cty";
    if (!quiet.saveCityAs(flag_path.c_str()) || disasters_slot(flag_path) != 2) {
        std::fprintf(stderr, "slot %d\n", disasters_slot(flag_path));
        return fail(8, "disasters off was not stored as code 2 in miscHist[64]");
    }
    Micropolis loaded_off;
    if (!loaded_off.loadCity(flag_path.c_str()) || loaded_off.enableDisasters || loaded_off.simSpeed != 0) {
        return fail(9, "a fresh engine did not restore disasters off and pause");
    }
    quiet.setEnableDisasters(true);
    quiet.setSpeed(2);
    if (!quiet.saveCityAs(flag_path.c_str()) || disasters_slot(flag_path) != 1) {
        return fail(10, "disasters on was not stored as code 1");
    }
    Micropolis loaded_on;
    if (!loaded_on.loadCity(flag_path.c_str()) || !loaded_on.enableDisasters) {
        return fail(11, "a fresh engine did not restore disasters on");
    }

    std::vector<char> classic = read_bytes(flag_path);
    if (classic.size() < 27120) {
        return fail(12, "flag fixture was shorter than a classic city");
    }
    classic.resize(27120);
    const std::size_t slot_off = 6u * 240u * 2u + 64u * 2u;
    classic[slot_off] = 0;
    classic[slot_off + 1] = 0;
    {
        std::ofstream out(flag_path, std::ios::binary);
        out.write(classic.data(), static_cast<std::streamsize>(classic.size()));
    }
    Micropolis loaded_old;
    if (!loaded_old.loadCity(flag_path.c_str()) || !loaded_old.enableDisasters ||
        loaded_old.miscHist[MISC_DISASTERS_SLOT] != 0) {
        return fail(13, "a pre-0.9-4 file with slot 0 did not keep disasters on");
    }
    const std::string dullsville = std::string(LUNDUKE_CITY_ASSET_DIR) + "/res/snro.111";
    Micropolis dull;
    if (!dull.loadFile(dullsville.c_str()) || !dull.enableDisasters ||
        dull.miscHist[MISC_DISASTERS_SLOT] == DISASTERS_FILE_OFF) {
        return fail(14, "Dullsville's leftover miscHist[64] turned disasters off");
    }

    CitySession paused;
    paused.new_city("Paused", 6);
    paused.set_speed(0);
    const std::string speed_path = "/tmp/lunduke-round3-speed.cty";
    if (!paused.save_city_as(speed_path)) {
        return fail(15, "could not save a paused city");
    }
    CitySession resumed;
    resumed.set_speed(2);
    if (!resumed.load_city(speed_path) || resumed.speed() != 0 || resumed.needs_save_prompt()) {
        std::fprintf(stderr, "speed %d prompt %d\n", resumed.speed(), resumed.needs_save_prompt() ? 1 : 0);
        return fail(16, "loading a paused city started it at the old session speed");
    }
    if (!resumed.disasters()) {
        return fail(17, "a city saved with disasters on loaded with them off");
    }

    CitySession options;
    options.new_city("Options", 7);
    if (options.needs_save_prompt()) {
        return fail(18, "a new city was already dirty");
    }
    options.set_tax(0);
    options.set_road_funding(0);
    options.set_auto_budget(false);
    options.set_auto_bulldoze(false);
    options.set_auto_goto(false);
    options.set_sound_enabled(false);
    options.set_disasters(false);
    if (!options.needs_save_prompt() || options.tax() != 0 || options.disasters()) {
        return fail(19, "tax, funding, or an option change was not unsaved");
    }
    if (hostile_review_session_probe(options, 3) != MAX_ROAD_EFFECT) {
        return fail(20, "could not stage full road maintenance");
    }
    options.set_road_funding(0);
    if (hostile_review_session_probe(options, 4) != MAX_ROAD_EFFECT) {
        return fail(21, "the road slider changed maintenance before the budget closed");
    }
    options.finish_budget_edit();
    if (hostile_review_session_probe(options, 4) != 0) {
        std::fprintf(stderr, "effect %d\n", hostile_review_session_probe(options, 4));
        return fail(22, "closing a menu budget at 0% roads left maintenance at full");
    }

    CitySession tax_year;
    tax_year.new_city("Taxes", 8);
    if (hostile_review_session_probe(tax_year, 8) != 8000) {
        return fail(23, "could not open the $8,000 tax year");
    }
    tax_year.set_tax(0);
    const CitySession::BudgetBook zero_tax = tax_year.budget();
    if (tax_year.tax() != 0 || zero_tax.taxes != 0) {
        std::fprintf(stderr, "tax %d collected %ld\n", tax_year.tax(), zero_tax.taxes);
        return fail(24, "the tax slider left this year's taxes unchanged");
    }
    tax_year.set_tax(7);
    if (hostile_review_session_probe(tax_year, 8) < 0) {
        return fail(25, "could not reopen the tax year after the rate change");
    }
    const CitySession::BudgetBook open_year = tax_year.budget();
    if (open_year.previous_funds != 8000 || open_year.cash_flow != 1600 || open_year.funds != 9600) {
        std::fprintf(stderr, "prev %ld flow %ld current %ld\n", open_year.previous_funds, open_year.cash_flow,
                     open_year.funds);
        return fail(26, "current funds was not previous funds plus the cash flow Close will post");
    }

    CitySession broke;
    broke.new_city("Broke", 9);
    if (hostile_review_session_probe(broke, 9) != 0) {
        return fail(27, "could not open the broke city's tax year");
    }
    const CitySession::BudgetBook clamped = broke.budget();
    const long shown_funds = clamped.funds;
    if (clamped.road_note.find("cut to") == std::string::npos || clamped.funds < 0 ||
        clamped.cash_flow != clamped.funds - 100) {
        std::fprintf(stderr, "note '%s' funds %ld flow %ld\n", clamped.road_note.c_str(), clamped.funds,
                     clamped.cash_flow);
        return fail(28, "the budget did not show the funding Close will actually post");
    }
    broke.finish_budget_edit();
    if (broke.funds() != shown_funds || broke.budget_pending() || broke.funds() < 0) {
        std::fprintf(stderr, "posted %ld actual %ld\n", shown_funds, broke.funds());
        return fail(29, "Close did not post the cash flow the window showed");
    }

    CitySession tools;
    tools.new_city("Tools", 10);
    const long tool_funds = tools.funds();
    const int serial_before = tools.message_serial();
    tools.use_tool(TOOL_RESIDENTIAL, 0, 8);
    const std::string off_map = tools.message();
    const int serial_once = tools.message_serial();
    tools.use_tool(TOOL_RESIDENTIAL, 0, 9);
    if (tools.needs_save_prompt() || tools.funds() != tool_funds ||
        off_map.find("off the map") == std::string::npos || tools.message() != off_map ||
        serial_once <= serial_before || tools.message_serial() <= serial_once) {
        std::fprintf(stderr, "msg '%s' serial %d %d %d\n", tools.message().c_str(), serial_before, serial_once,
                     tools.message_serial());
        return fail(30, "an off-map zone was silent or marked the city unsaved");
    }
    const NoticeDecision repeated = decide_notice(tools.message_serial(), serial_once, true, false);
    const NoticeDecision waiting = decide_notice(serial_once, serial_once, false, false);
    const NoticeDecision hinted = decide_notice(serial_once, serial_once, true, false);
    if (!repeated.show_notice || waiting.show_hint || waiting.show_notice || !hinted.show_hint) {
        return fail(31, "a repeated notice would not return after the hint replaced it");
    }
    if (hostile_review_session_probe(tools, 11) != 0) {
        return fail(32, "could not stage open water");
    }
    tools.use_tool(TOOL_ROAD, 20, 20);
    if (tools.needs_save_prompt() || tools.message().find("anchor") == std::string::npos) {
        std::fprintf(stderr, "bridge msg '%s'\n", tools.message().c_str());
        return fail(33, "a road with nowhere to anchor a bridge was silent or marked unsaved");
    }
    if (hostile_review_session_probe(tools, 10) != 0) {
        return fail(34, "could not stage a bridge");
    }
    const long before_bridge = tools.funds();
    tools.use_tool(TOOL_ROAD, 10, 10);
    const ToolDef *roads = road_tool();
    if (roads == nullptr || std::string(roads->hint).find("Bridge: $50") == std::string::npos ||
        tools.message().find("Bridge: $50") == std::string::npos || tools.funds() != before_bridge - 50 ||
        !tools.needs_save_prompt()) {
        std::fprintf(stderr, "hint '%s' msg '%s' funds %ld\n", roads != nullptr ? roads->hint : "",
                     tools.message().c_str(), tools.funds());
        return fail(35, "a bridge did not cost $50 or the road hint omitted that price");
    }

    Micropolis graph;
    graph.generateSomeCity(1);
    graph.setSpeed(3);
    bool recorded = false;
    for (int i = 0; i < 400 && !recorded; ++i) {
        graph.cashFlow = 50000;
        graph.simTick();
        recorded = graph.cashFlowHistory(HISTORY_SCALE_SHORT, 0) == 50000;
    }
    if (!recorded || graph.cashFlowHistory(HISTORY_SCALE_SHORT, 0) != 50000 ||
        graph.getHistory(HISTORY_TYPE_MONEY, HISTORY_SCALE_SHORT, 0) != 255 ||
        !graph.cashFlowHistoryExact(HISTORY_SCALE_SHORT, 0)) {
        std::fprintf(stderr, "flow %ld byte %d\n",
                     static_cast<long>(graph.cashFlowHistory(HISTORY_SCALE_SHORT, 0)),
                     graph.getHistory(HISTORY_TYPE_MONEY, HISTORY_SCALE_SHORT, 0));
        return fail(36, "the cash-flow graph still reads the capped history byte");
    }
    if (graph_legend_caption("Cash flow", GraphLegendKind::CashFlow, 50000) != "Cash flow: $50,000") {
        return fail(37, "the cash-flow legend still caps a $50,000 year");
    }

    const std::string blocked = "/tmp/lunduke-round3-blocked.cty";
    std::remove(blocked.c_str());
    if (mkdir(blocked.c_str(), 0755) != 0) {
        return fail(38, "could not create a directory where a city file should be");
    }
    CitySession saver;
    saver.new_city("Save", 11);
    saver.set_tax(3);
    if (!saver.needs_save_prompt() || saver.save_city_as(blocked) ||
        saver.message().find("Could not save") == std::string::npos ||
        saver.message().find(blocked) == std::string::npos || !saver.needs_save_prompt()) {
        std::fprintf(stderr, "save msg '%s'\n", saver.message().c_str());
        rmdir(blocked.c_str());
        return fail(39, "saving onto a directory did not report the path and stay unsaved");
    }
    rmdir(blocked.c_str());
    const std::string link = "/tmp/lunduke-round3-link.cty";
    std::remove(link.c_str());
    if (symlink(path.c_str(), link.c_str()) != 0 || saver.save_city_as(link) ||
        saver.message().find("symlink") == std::string::npos) {
        std::fprintf(stderr, "link msg '%s'\n", saver.message().c_str());
        std::remove(link.c_str());
        return fail(40, "saving onto a symlink did not say so");
    }
    std::remove(link.c_str());

    CitySession scenario;
    if (!scenario.load_scenario(SC_DULLSVILLE) || !scenario.disasters()) {
        return fail(41, "Dullsville did not load with disasters on");
    }
    if (hostile_review_session_probe(scenario, 5) != 0 || scenario.take_scenario_outcome() != 1) {
        return fail(42, "winning a scenario did not pause or record the win");
    }
    CitySession lost;
    if (!lost.load_scenario(SC_DULLSVILLE)) {
        return fail(43, "could not reload Dullsville for a loss");
    }
    if (hostile_review_session_probe(lost, 6) != 0 || lost.take_scenario_outcome() != -1) {
        return fail(44, "losing a scenario did not pause or record the loss");
    }

    CitySession named;
    named.new_city("Harbor", 12);
    named.new_city("   ", 13);
    if (named.city_name() != "Harbor" || named.message().find("not only spaces") == std::string::npos) {
        std::fprintf(stderr, "name '%s' msg '%s'\n", named.city_name().c_str(), named.message().c_str());
        return fail(45, "a whitespace name replaced the city or kept the previous name silently");
    }
    named.rename_city(" \t ");
    if (named.city_name() != "Harbor") {
        return fail(46, "renaming to spaces changed the city");
    }
    named.rename_city("!!!");
    if (named.city_name() != "!!!" || !CitySession::name_is_usable("!!!") ||
        CitySession::name_is_usable("   ")) {
        return fail(47, "a name of exclamation marks was rejected, or spaces were accepted");
    }

    std::remove(path.c_str());
    std::remove(flag_path.c_str());
    std::remove(speed_path.c_str());
    return 0;
}

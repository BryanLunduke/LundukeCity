// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless regressions for the round-6 review.
// The budget window, the announcement close, and the overlay click are in ui_round6.

#include "city_session.hpp"
#include "tools.hpp"

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
    // Finding 2: the eight classic goals, and the years still left.
    static const char *kGoals[] = {
        "Turn this city into a Metropolis (more than 100,000 people) within 30 years.",
        "Reach Metropolis (more than 100,000 people) within 5 years.",
        "Reach Metropolis (more than 100,000 people) within 5 years.",
        "Bring average traffic below 80 within 10 years.",
        "Raise the city score above 500 within 5 years.",
        "Bring average crime below 60 within 10 years.",
        "Raise the city score above 500 within 5 years.",
        "Raise the city score above 500 within 10 years.",
    };
    static const int kYears[] = {30, 5, 5, 10, 5, 10, 5, 10};
    if (CitySession::kScenarioCount != 8) {
        return fail(1, "the scenario catalog is no longer the eight classic cities");
    }
    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        if (std::string(def.goal) != kGoals[i] || def.years != kYears[i] || def.summary == nullptr ||
            def.summary[0] == '\0') {
            std::fprintf(stderr, "%s goal '%s' years %d\n", def.name, def.goal, def.years);
            return fail(2, "a scenario goal is not the classic win condition and deadline");
        }
    }

    {
        CitySession dull;
        if (!dull.load_scenario(CitySession::scenario_def(0).id) || dull.message() != "Playing Dullsville." ||
            dull.scenario_years_left() != 30 || dull.scenario_id() != CitySession::scenario_def(0).id) {
            std::fprintf(stderr, "years %d id %d msg '%s'\n", dull.scenario_years_left(), dull.scenario_id(),
                         dull.message().c_str());
            return fail(3, "Dullsville did not start with 30 years and the playing notice");
        }
        const std::string progress = dull.scenario_progress();
        if (progress.find("30 years left") == std::string::npos ||
            progress.find(kGoals[0]) == std::string::npos) {
            std::fprintf(stderr, "progress '%s'\n", progress.c_str());
            return fail(4, "the running scenario does not state the goal and the time left");
        }
        dull.set_speed(3);
        int guard = 0;
        while (dull.scenario_years_left() == 30 && guard < 4000) {
            dull.tick();
            ++guard;
        }
        if (dull.scenario_years_left() != 29 || guard == 0) {
            std::fprintf(stderr, "left %d ticks %d\n", dull.scenario_years_left(), guard);
            return fail(5, "a year of simulation did not reduce the time left");
        }
        const std::string won = dull.scenario_outcome_text(true);
        const std::string lost = dull.scenario_outcome_text(false);
        if (won.find("Dullsville is won.") == std::string::npos || won.find("The goal was met:") == std::string::npos ||
            won.find(kGoals[0]) == std::string::npos ||
            won.find("paused for this announcement") == std::string::npos ||
            won.find("Keep playing continues at the previous speed") == std::string::npos ||
            won.find("cancelling the scenario list, leaves the city paused") == std::string::npos ||
            lost.find("The goal was missed:") == std::string::npos) {
            std::fprintf(stderr, "won '%s'\n", won.c_str());
            return fail(6, "the outcome dialog does not name the goal or the paused close");
        }
    }

    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        CitySession city;
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        if (!city.load_scenario(def.id) || city.scenario_years_left() != def.years ||
            city.scenario_progress().find(def.goal) == std::string::npos) {
            std::fprintf(stderr, "%s left %d\n", def.name, city.scenario_years_left());
            return fail(7, "a scenario deadline does not match its classic year count");
        }
    }

    // Finding 1: estimates before the first January, including every scenario.
    // A receipt that was stored still wins over the estimate.
    {
        CitySession fresh;
        fresh.new_city("Fresh", 21);
        const CitySession::BudgetBook book = fresh.budget();
        const int projected = hostile_review_session_probe(fresh, 20);
        if (book.taxes_known || !book.estimates || (projected != 1 && projected != 2)) {
            std::fprintf(stderr, "known %d estimates %d road %ld tax %ld\n", book.taxes_known ? 1 : 0,
                         book.estimates ? 1 : 0, book.road_need, book.taxes);
            return fail(8, "a new city still treats a missing January as a zero receipt");
        }
        if (book.road_need < 0 || book.police_need < 0 || book.fire_need < 0 || book.taxes < 0) {
            return fail(9, "an estimate went negative");
        }
    }
    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        CitySession city;
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        if (!city.load_scenario(def.id) || hostile_review_session_probe(city, 20) != 2) {
            const CitySession::BudgetBook book = city.budget();
            std::fprintf(stderr, "%s known %d est %d road %ld police %ld fire %ld tax %ld\n", def.name,
                         book.taxes_known ? 1 : 0, book.estimates ? 1 : 0, book.road_need, book.police_need,
                         book.fire_need, book.taxes);
            return fail(10, "a scenario priced its departments at the empty receipt");
        }
    }
    {
        CitySession plain;
        plain.new_city("Plain", 22);
        if (plain.scenario_id() != 0 || !plain.scenario_progress().empty() || plain.scenario_years_left() != -1) {
            return fail(11, "a new city claimed a scenario goal");
        }
    }

    // Finding 4: a slider that reports 0 again keeps the cut sentence.
    {
        CitySession taxed;
        taxed.new_city("Taxed", 12);
        if (hostile_review_session_probe(taxed, 16) != 100) {
            return fail(12, "could not open the tax-plus-cash cut");
        }
        const CitySession::BudgetBook book = taxed.budget();
        if (book.fire_percent != 0 || book.fire_note.find("earlier department") == std::string::npos ||
            book.road_percent != 42 || book.road_note.find("cut to 42%") == std::string::npos) {
            std::fprintf(stderr, "fire %d '%s' road %d '%s'\n", book.fire_percent, book.fire_note.c_str(),
                         book.road_percent, book.road_note.c_str());
            return fail(13, "the tax year did not cut fire to 0% with an explanation");
        }
        const std::string fire_note = book.fire_note;
        const std::string road_note = book.road_note;
        taxed.set_fire_funding(0);
        taxed.set_road_funding(42);
        const CitySession::BudgetBook again = taxed.budget();
        if (again.fire_note != fire_note || again.fire_percent != 0 || again.road_note != road_note ||
            again.road_percent != 42) {
            std::fprintf(stderr, "fire '%s' road '%s'\n", again.fire_note.c_str(), again.road_note.c_str());
            return fail(14, "reporting the percent already showing, including 0, cleared the cut sentence");
        }
        taxed.set_fire_funding(40);
        if (taxed.budget().fire_percent != 40) {
            return fail(15, "a real change away from 0% did not move the slider");
        }
        // A real move to 0% is a new request. Reporting that same 0 again
        // must not rewrite the sentence the first request produced.
        taxed.set_fire_funding(0);
        const CitySession::BudgetBook at_zero = taxed.budget();
        taxed.set_fire_funding(0);
        if (at_zero.fire_percent != 0 || taxed.budget().fire_percent != 0 ||
            taxed.budget().fire_note != at_zero.fire_note) {
            std::fprintf(stderr, "moved '%s' then '%s'\n", at_zero.fire_note.c_str(),
                         taxed.budget().fire_note.c_str());
            return fail(16, "reporting 0 again after a real return to 0% changed the cut sentence");
        }
    }

    // Tooltips: every tool's hint is its name, and a priced tool names the cost.
    for (int i = 0; i < kToolCount; ++i) {
        const ToolDef *tool = tool_by_index(i);
        const std::string hint = tool->hint == nullptr ? "" : tool->hint;
        if (hint.find(tool->name) == std::string::npos) {
            std::fprintf(stderr, "tool %s hint '%s'\n", tool->name, hint.c_str());
            return fail(17, "a tool tooltip does not include the tool name");
        }
        if (tool->cost > 0 && hint.find('$') == std::string::npos) {
            return fail(18, "a priced tool tooltip does not include the cost");
        }
    }

    return 0;
}

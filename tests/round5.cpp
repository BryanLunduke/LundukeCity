// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless regressions for the round-5 review.
// Budget buttons and the tax-year close stay in ui_round4.

#include "city_session.hpp"
#include "tools.hpp"

#include "micropolis.h"

#include <cstdio>
#include <fstream>
#include <string>
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

short classic_short(const std::vector<char> &data, int slot)
{
    const std::size_t offset = 6u * 240u * 2u + static_cast<std::size_t>(slot) * 2u;
    if (offset + 1 >= data.size()) {
        return -1;
    }
    const unsigned int hi = static_cast<unsigned char>(data[offset]);
    const unsigned int lo = static_cast<unsigned char>(data[offset + 1]);
    return static_cast<short>((hi << 8) | lo);
}

// What a reader that only knows the classic Mac long would return.
long classic_mac_long(const std::vector<char> &data, int slot)
{
    const unsigned int low = static_cast<unsigned short>(classic_short(data, slot));
    const unsigned int high = static_cast<unsigned short>(classic_short(data, slot + 1));
    const unsigned int swapped = low | (high << 16);
    const unsigned int bits = ((swapped & 0xffffu) << 16) | (swapped >> 16);
    return static_cast<long>(static_cast<int>(bits));
}

unsigned map_hash(const Micropolis &sim)
{
    unsigned hash = 2166136261u;
    for (int x = 0; x < WORLD_W; ++x) {
        for (int y = 0; y < WORLD_H; ++y) {
            hash ^= sim.map[x][y];
            hash *= 16777619u;
        }
    }
    return hash;
}

const char *kScenarios[] = {
    "snro.111", "snro.222", "snro.333", "snro.444",
    "snro.555", "snro.666", "snro.777", "snro.888",
};

} // namespace

// Friends of Micropolis. These stay in the test binary.
Quad lunduke_city_round5_population(Micropolis &sim)
{
    return sim.getPopulation();
}

CityClass lunduke_city_round5_city_class(Micropolis &sim, Quad population)
{
    return sim.getCityClass(population);
}

void lunduke_city_round5_take10_census(Micropolis &sim)
{
    sim.take10Census();
}

void lunduke_city_round5_capture_census(Micropolis &sim)
{
    sim.captureCensusSnapshot();
}

int main()
{
    // Finding 4: the residential counter itself no longer wraps at 16 bits.
    {
        Micropolis sim;
        sim.generateSomeCity(3);
        sim.resPop = 0;
        for (int i = 0; i < 820; ++i) {
            sim.resPop += 40;
        }
        const Quad people820 = lunduke_city_round5_population(sim);
        if (sim.resPop != 32800 || people820 != 32800LL * 20LL ||
            lunduke_city_round5_city_class(sim, people820) != CC_MEGALOPOLIS) {
            std::fprintf(stderr, "res %lld pop %lld\n", (long long)sim.resPop, (long long)people820);
            return fail(1, "adding top-density housing 820 times still wraps the census");
        }
        sim.comPop = 65536;
        sim.indPop = 70000;
        const Quad people = lunduke_city_round5_population(sim);
        if (people <= 0 || sim.resPop != 32800) {
            std::fprintf(stderr, "people %lld\n", (long long)people);
            return fail(2, "a census past 65535 still collapses the population");
        }
    }

    // Findings 4 and 5: wide values round-trip, classic fields saturate.
    const std::string wide_path = "/tmp/lunduke-round5-wide.cty";
    {
        Micropolis sim;
        sim.generateSomeCity(4);
        sim.resPop = 32800;
        sim.comPop = 65536;
        sim.indPop = 400000;
        lunduke_city_round5_take10_census(sim);
        lunduke_city_round5_capture_census(sim);
        sim.taxReceiptKnown = true;
        sim.taxFund = 2000;
        sim.roadFund = 5000;
        sim.policeFund = 300;
        sim.fireFund = 400;
        sim.setFunds(3000000000LL);
        sim.setEnableDisasters(false);
        if (sim.resHist[0] != 4100 || sim.comHist[0] != 32767 || sim.indHist[0] != 32767) {
            std::fprintf(stderr, "hist %d %d %d\n", sim.resHist[0], sim.comHist[0], sim.indHist[0]);
            return fail(3, "the 16-bit history sample wrapped instead of saturating");
        }
        if (!sim.saveCityAs(wide_path.c_str())) {
            return fail(4, "could not save the wide city");
        }
        const std::vector<char> bytes = read_bytes(wide_path);
        if (classic_short(bytes, 2) != 32767 || classic_short(bytes, 3) != 32767 ||
            classic_short(bytes, 4) != 32767 || classic_short(bytes, 2) < 0) {
            std::fprintf(stderr, "classic census %d %d %d\n", classic_short(bytes, 2), classic_short(bytes, 3),
                         classic_short(bytes, 4));
            return fail(5, "an old-format reader would see a wrapped census");
        }
        const long classic_funds = classic_mac_long(bytes, 50);
        if (classic_funds != 2147483647L || classic_funds < 0) {
            std::fprintf(stderr, "classic funds %ld\n", classic_funds);
            return fail(6, "an old-format reader would load a surplus as debt");
        }
        if (classic_short(bytes, MISC_DISASTERS_SLOT) != DISASTERS_FILE_OFF) {
            return fail(7, "the wide trailer collided with the disasters flag");
        }
        Micropolis loaded;
        if (!loaded.loadCity(wide_path.c_str()) || loaded.totalFunds != 3000000000LL || loaded.resPop != 32800 ||
            loaded.comPop != 65536 || loaded.indPop != 400000 || loaded.taxFund != 2000 || loaded.roadFund != 5000 ||
            loaded.policeFund != 300 || loaded.fireFund != 400 || !loaded.taxReceiptKnown || loaded.enableDisasters ||
            lunduke_city_round5_population(loaded) <= 0 || loaded.indHist[0] != 32767) {
            std::fprintf(stderr, "funds %lld res %lld com %lld ind %lld tax %lld road %lld pop %lld hist %d\n",
                         (long long)loaded.totalFunds, (long long)loaded.resPop, (long long)loaded.comPop,
                         (long long)loaded.indPop, (long long)loaded.taxFund, (long long)loaded.roadFund,
                         (long long)lunduke_city_round5_population(loaded), loaded.indHist[0]);
            return fail(8, "the wide census, treasury, or January receipt did not reload");
        }
        CitySession session;
        if (!session.load_city(wide_path) || !session.budget().taxes_known || session.budget().taxes != 2000 ||
            session.budget().road_need != 5000 || session.budget().police_need != 300 ||
            session.budget().fire_need != 400 || session.funds() != 3000000000L ||
            session.evaluation().population != static_cast<long>(lunduke_city_round5_population(loaded))) {
            std::fprintf(stderr, "book tax %ld known %d road %ld pop %ld funds %ld\n", session.budget().taxes,
                         session.budget().taxes_known ? 1 : 0, session.budget().road_need,
                         session.evaluation().population, session.funds());
            return fail(9, "the budget and evaluation still show the wrapped or missing numbers");
        }
    }

    // The first dollar past 2^31, and a debt the classic field cannot hold.
    {
        Micropolis rich;
        rich.generateSomeCity(5);
        rich.setFunds(2147483648LL);
        const std::string path = "/tmp/lunduke-round5-edge.cty";
        if (!rich.saveCityAs(path.c_str())) {
            return fail(10, "could not save the 2^31 treasury");
        }
        const long classic_funds = classic_mac_long(read_bytes(path), 50);
        Micropolis loaded;
        if (classic_funds != 2147483647L || classic_funds < 0 || !loaded.loadCity(path.c_str()) ||
            loaded.totalFunds != 2147483648LL) {
            std::fprintf(stderr, "classic %ld loaded %lld\n", classic_funds, (long long)loaded.totalFunds);
            return fail(11, "funds at 2^31 loaded as debt or the classic field did not saturate");
        }
        rich.setFunds(-3000000000LL);
        if (!rich.saveCityAs(path.c_str())) {
            return fail(12, "could not save a treasury below 32 bits");
        }
        const long classic_debt = classic_mac_long(read_bytes(path), 50);
        Micropolis debt;
        if (classic_debt != static_cast<long>(-2147483647L - 1) || !debt.loadCity(path.c_str()) ||
            debt.totalFunds != -3000000000LL) {
            std::fprintf(stderr, "classic debt %ld loaded %lld\n", classic_debt, (long long)debt.totalFunds);
            return fail(13, "a deep debt wrapped through the classic field");
        }
        std::remove(path.c_str());
    }

    // A value the classic field can hold is what both readers see.
    {
        Micropolis sim;
        sim.generateSomeCity(6);
        sim.setFunds(123456);
        sim.resPop = 1000;
        sim.comPop = 20;
        sim.indPop = 10;
        lunduke_city_round5_capture_census(sim);
        const std::string path = "/tmp/lunduke-round5-fit.cty";
        if (!sim.saveCityAs(path.c_str()) || classic_mac_long(read_bytes(path), 50) != 123456L) {
            return fail(14, "a treasury inside 32 bits was not stored in the classic field");
        }
        Micropolis loaded;
        if (!loaded.loadCity(path.c_str()) || loaded.totalFunds != 123456 || loaded.resPop != 1000) {
            std::fprintf(stderr, "funds %lld res %lld\n", (long long)loaded.totalFunds, (long long)loaded.resPop);
            return fail(15, "a census and treasury that fit the classic fields did not round-trip");
        }
        std::remove(path.c_str());
    }

    // Old save round-trip, and the eight scenarios, stay on the classic fields.
    {
        Micropolis sim;
        sim.generateSomeCity(7);
        sim.setFunds(123456);
        sim.setCityTax(9);
        sim.setSpeed(0);
        sim.setEnableDisasters(false);
        const std::string path = "/tmp/lunduke-round5-classic.cty";
        if (!sim.saveCityAs(path.c_str())) {
            return fail(16, "could not save the classic round-trip city");
        }
        std::vector<char> classic = read_bytes(path);
        if (classic.size() < 27120) {
            return fail(17, "the saved city is shorter than a classic file");
        }
        classic.resize(27120);
        {
            std::ofstream out(path, std::ios::binary);
            out.write(classic.data(), static_cast<std::streamsize>(classic.size()));
        }
        Micropolis loaded;
        if (!loaded.loadCity(path.c_str()) || loaded.totalFunds != 123456 || loaded.cityTax != 9 ||
            loaded.simSpeed != 0 || loaded.enableDisasters || loaded.taxReceiptKnown || loaded.resPop < 0 ||
            map_hash(loaded) != map_hash(sim)) {
            std::fprintf(stderr, "funds %lld tax %d speed %lld disasters %d res %lld\n",
                         (long long)loaded.totalFunds, loaded.cityTax, (long long)loaded.simSpeed,
                         loaded.enableDisasters ? 1 : 0, (long long)loaded.resPop);
            return fail(18, "a classic file without the wide trailer did not load unchanged");
        }
        CitySession session;
        if (!session.load_city(path) || session.budget().taxes_known || session.funds() != 123456 ||
            session.tax() != 9 || session.speed() != 0 || session.disasters()) {
            return fail(19, "a classic file grew a tax receipt or changed the city");
        }
        if (!loaded.saveCityAs(path.c_str()) || !Micropolis().loadCity(path.c_str())) {
            return fail(20, "saving the loaded classic city failed");
        }
        Micropolis again;
        if (!again.loadCity(path.c_str()) || again.totalFunds != 123456 || again.cityTax != 9 || again.simSpeed != 0 ||
            again.enableDisasters || map_hash(again) != map_hash(sim)) {
            return fail(21, "the classic city changed across a second save");
        }
        std::remove(path.c_str());
    }

    {
        const std::string root = std::string(LUNDUKE_CITY_ASSET_DIR) + "/res/";
        for (int i = 0; i < 8; ++i) {
            const std::string file = root + kScenarios[i];
            const std::vector<char> bytes = read_bytes(file);
            if (bytes.size() != 27120) {
                std::fprintf(stderr, "%s size %zu\n", file.c_str(), bytes.size());
                return fail(22, "a bundled scenario is no longer a classic file");
            }
            Micropolis sim;
            if (!sim.loadFile(file.c_str()) || sim.totalFunds < 0 || sim.resPop < 0 || sim.comPop < 0 ||
                sim.indPop < 0 || !sim.enableDisasters || sim.taxReceiptKnown) {
                std::fprintf(stderr, "%s funds %lld res %lld\n", kScenarios[i], (long long)sim.totalFunds,
                             (long long)sim.resPop);
                return fail(23, "a bundled scenario did not load from its classic fields");
            }
        }
        CitySession session;
        if (!session.load_scenario(SC_DULLSVILLE) || session.city_name() != "Dullsville" || session.funds() != 5000 ||
            session.budget().taxes_known) {
            return fail(24, "Dullsville did not load at $5,000 with no stored receipt");
        }
        for (int i = 0; i < CitySession::kScenarioCount; ++i) {
            const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
            const long expect = def.id == SC_DULLSVILLE ? 5000 : 20000;
            if (!session.load_scenario(def.id) || session.city_name() != def.name || session.funds() != expect ||
                !session.disasters()) {
                std::fprintf(stderr, "%s funds %ld\n", def.name, session.funds());
                return fail(25, "a scenario load changed funds, the name, or disasters");
            }
        }
    }

    // Findings 1 and 6: the cut sentence uses this year's taxes, and a
    // slider that stays on the same percent keeps it.
    {
        CitySession broke;
        broke.new_city("Broke", 11);
        if (hostile_review_session_probe(broke, 14) != 100) {
            return fail(26, "could not open the zero-tax cut");
        }
        const CitySession::BudgetBook zero = broke.budget();
        if (zero.road_note.find("cut to " + std::to_string(zero.road_percent) + "%") == std::string::npos ||
            zero.road_note.find("taxes ($0)") == std::string::npos ||
            zero.road_note.find("$100") == std::string::npos ||
            zero.road_note.find("cannot cover") == std::string::npos ||
            zero.fire_note.find("earlier department") == std::string::npos || !zero.police_note.empty()) {
            std::fprintf(stderr, "road '%s' fire '%s' police '%s'\n", zero.road_note.c_str(), zero.fire_note.c_str(),
                         zero.police_note.c_str());
            return fail(27, "a zero-tax cut still names only the cash balance");
        }
        const std::string kept = zero.road_note;
        broke.set_road_funding(zero.road_percent);
        if (broke.budget().road_note != kept || broke.budget().road_percent != zero.road_percent) {
            std::fprintf(stderr, "after '%s'\n", broke.budget().road_note.c_str());
            return fail(28, "a slider move that stays on the same percent erased the cut sentence");
        }

        CitySession taxed;
        taxed.new_city("Taxed", 12);
        if (hostile_review_session_probe(taxed, 16) != 100) {
            return fail(29, "could not open the tax-plus-cash cut");
        }
        const CitySession::BudgetBook book = taxed.budget();
        if (book.road_percent != 42 || book.taxes != 2000 ||
            book.road_note.find("cut to 42%") == std::string::npos ||
            book.road_note.find("taxes ($2,000)") == std::string::npos ||
            book.road_note.find("the $100 on hand cannot cover 100%") == std::string::npos ||
            book.fire_note.find("earlier department") == std::string::npos ||
            book.fire_note.find("taxes ($2,000)") == std::string::npos) {
            std::fprintf(stderr, "pct %d road '%s' fire '%s'\n", book.road_percent, book.road_note.c_str(),
                         book.fire_note.c_str());
            return fail(30, "the cut sentence ignored this year's taxes");
        }
        const std::string taxed_note = book.road_note;
        taxed.set_road_funding(42);
        if (taxed.budget().road_note != taxed_note) {
            std::fprintf(stderr, "wiggle '%s'\n", taxed.budget().road_note.c_str());
            return fail(31, "setting the road slider to the percent it already shows cleared the sentence");
        }
        taxed.set_road_funding(100);
        if (taxed.budget().road_note.find("cannot cover 100%") == std::string::npos) {
            std::fprintf(stderr, "raised '%s'\n", taxed.budget().road_note.c_str());
            return fail(32, "raising the slider above the taxes and cash did not keep a cut sentence");
        }
    }

    // Finding 3: a file with no receipt does not pretend January collected $0,
    // and a file that has one reloads the dollars. Covered above for the wide
    // city and the classic strip. A fresh city has no receipt until January.
    {
        CitySession fresh;
        fresh.new_city("Fresh", 13);
        if (fresh.budget().taxes_known) {
            return fail(33, "a new city already claimed a January receipt");
        }
    }

    // Finding 2: the announcement and Keep playing describe the same clock.
    {
        CitySession won;
        if (!won.load_scenario(SC_DULLSVILLE)) {
            return fail(34, "could not load Dullsville for the announcement");
        }
        won.set_speed(3);
        if (hostile_review_session_probe(won, 5) != 0 || won.speed() != 0) {
            return fail(35, "the win did not pause for the announcement");
        }
        const std::string text = won.scenario_outcome_text(true);
        if (text.find("Dullsville is won.") == std::string::npos ||
            text.find("paused for this announcement") == std::string::npos ||
            text.find("Keep playing continues at the previous speed") == std::string::npos) {
            std::fprintf(stderr, "text '%s'\n", text.c_str());
            return fail(36, "the win dialog still says the city stays paused");
        }
        won.stay_paused_after_outcome();
        if (won.speed() != 0 || won.outcome_resume_speed() != 3 || !won.outcome_pause_pending()) {
            return fail(37, "Stay paused started the clock or forgot the previous speed");
        }
        if (!won.load_scenario(SC_BERN) || won.speed() != 3) {
            std::fprintf(stderr, "next %d\n", won.speed());
            return fail(38, "Stay paused made the next scenario start at the wrong speed");
        }

        CitySession lost;
        if (!lost.load_scenario(SC_HAMBURG)) {
            return fail(39, "could not load Hamburg for the loss");
        }
        lost.set_speed(1);
        hostile_review_session_probe(lost, 6);
        const std::string lost_text = lost.scenario_outcome_text(false);
        if (lost_text.find("Hamburg is lost.") == std::string::npos ||
            lost_text.find("Keep playing continues") == std::string::npos) {
            return fail(40, "the loss dialog does not say Keep playing continues");
        }
        lost.resume_after_outcome();
        if (lost.speed() != 1 || lost.outcome_pause_pending()) {
            return fail(41, "Keep playing did not run at the speed from before the loss");
        }
    }

    // Finding 7: a power line that cannot land is not a bridge.
    {
        CitySession crossing;
        crossing.new_city("Wire", 14);
        if (hostile_review_session_probe(crossing, 11) != 0) {
            return fail(42, "could not stage open water");
        }
        const long funds = crossing.funds();
        crossing.use_tool(TOOL_ROAD, 20, 20);
        const std::string road = crossing.message();
        crossing.use_tool(TOOL_RAILROAD, 20, 20);
        const std::string rail = crossing.message();
        crossing.use_tool(TOOL_WIRE, 20, 20);
        const std::string wire = crossing.message();
        if (crossing.funds() != funds || road.find("anchor") == std::string::npos ||
            road.find("bridge") == std::string::npos || rail.find("anchor") == std::string::npos ||
            wire != "No place to land a power line." || wire.find("bridge") != std::string::npos ||
            wire.find("anchor") != std::string::npos) {
            std::fprintf(stderr, "road '%s' rail '%s' wire '%s'\n", road.c_str(), rail.c_str(), wire.c_str());
            return fail(43, "a power line with nowhere to land still uses the bridge sentence");
        }
    }

    std::remove(wide_path.c_str());
    return 0;
}

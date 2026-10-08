// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "messages.hpp"

namespace {

const char *kMessages[] = {
    nullptr,
    "More residential zones needed.",
    "More commercial zones needed.",
    "More industrial zones needed.",
    "More roads needed.",
    "More rail needed.",
    "Build a power plant.",
    "Residents demand a stadium.",
    "Industry needs a seaport.",
    "Commerce needs an airport.",
    "Pollution is very high.",
    "Crime is very high.",
    "Traffic jams reported.",
    "Citizens demand a fire department.",
    "Citizens demand a police department.",
    "Blackouts reported. Check power.",
    "The tax rate is too high.",
    "Roads are deteriorating from lack of funds.",
    "Fire departments need funding.",
    "Police departments need funding.",
    "Fire reported.",
    "A monster has been sighted.",
    "A tornado has been reported.",
    "An earthquake has been reported.",
    "A plane has crashed.",
    "A ship has wrecked.",
    "A train has crashed.",
    "A helicopter has crashed.",
    "Unemployment is high.",
    "The city has run out of funds.",
    "Firebombing reported.",
    "More parks needed.",
    "An explosion has been detected.",
    "Not enough funds to build that.",
    "Bulldoze that area first.",
    "The population has reached 2,000.",
    "The population has reached 10,000.",
    "The population has reached 50,000.",
    "The population has reached 100,000.",
    "The population has reached 500,000.",
    "Brownouts. Build another power plant.",
    "Heavy traffic reported.",
    "Flooding reported.",
    "A nuclear meltdown has occurred.",
    "Riots reported.",
    "Started a new city.",
    "Loaded a saved city.",
    "Scenario won.",
    "Scenario lost.",
    "About this simulation.",
    "Dullsville scenario.",
    "San Francisco scenario.",
    "Hamburg scenario.",
    "Bern scenario.",
    "Tokyo scenario.",
    "Detroit scenario.",
    "Boston scenario.",
    "Rio scenario.",
    "Cannot build here.",
    "That would be off the map.",
    "No place to anchor a bridge.",
    "Bridge: $50",
};

const char *kCategories[] = {
    "Clear",
    "Water",
    "Trees",
    "Rubble",
    "Flood",
    "Radioactive waste",
    "Fire",
    "Road",
    "Power",
    "Rail",
    "Residential",
    "Commercial",
    "Industrial",
    "Seaport",
    "Airport",
    "Coal power",
    "Fire department",
    "Police department",
    "Stadium",
    "Nuclear power",
    "Bridge",
    "Radar",
    "Fountain",
    "Industrial",
    "Stadium",
    "Bridge",
    "Open land",
    "Open land",
};

// doZoneStatus() passes stri.202 category ids plus one, not one shared
// 1–20 scale. Each field occupies four adjacent ids:
//   population 1–4, land value 5–8, crime 9–12,
//   pollution 13–16, growth 17–20.
const char *band(int value, int origin, const char *low, const char *mid, const char *high,
                 const char *very_high)
{
    int index = value - origin;
    if (index < 0) {
        index = 0;
    }
    if (index > 3) {
        index = 3;
    }
    const char *words[] = {low, mid, high, very_high};
    return words[index];
}

} // namespace

std::string message_for_number(int number)
{
    const int count = static_cast<int>(sizeof(kMessages) / sizeof(kMessages[0]));
    if (number <= 0 || number >= count || kMessages[number] == nullptr) {
        return "Notice.";
    }
    return kMessages[number];
}

std::string zone_status_text(int tile_category, int population, int land_value,
                             int crime, int pollution, int growth)
{
    const int cat_count = static_cast<int>(sizeof(kCategories) / sizeof(kCategories[0]));
    const char *kind = "Land";
    if (tile_category == 29 || tile_category == 1) {
        kind = "Clear";
    } else if (tile_category >= 1 && tile_category <= cat_count) {
        kind = kCategories[tile_category - 1];
    }

    const char *pop = band(population, 1, "sparse", "settled", "busy", "packed");
    const char *land = band(land_value, 5, "low", "modest", "high", "prime");
    const char *crm = band(crime, 9, "safe", "light", "moderate", "dangerous");
    const char *pol = band(pollution, 13, "clean", "light", "moderate", "heavy");
    const char *gro = band(growth, 17, "declining", "stable", "growing", "booming");

    return std::string(kind) + " — population " + pop + ", land value " + land +
           ", crime " + crm + ", pollution " + pol + ", growth " + gro;
}

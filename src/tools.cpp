// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "tools.hpp"

#include "micropolis.h"

#include <cassert>

const ToolDef kTools[] = {
    {TOOL_BULLDOZER, "Bulldozer", 1, "Bulldozer: $1"},
    {TOOL_ROAD, "Roads", 10, "Roads: $10. Bridge: $50"},
    {TOOL_RAILROAD, "Rail", 20, "Rail: $20. Rail bridge: $100"},
    {TOOL_WIRE, "Power lines", 5, "Power lines: $5. Underwater wire: $25"},
    {TOOL_PARK, "Park", 10, "Park: $10"},
    {TOOL_RESIDENTIAL, "Residential", 100, "Residential: $100"},
    {TOOL_COMMERCIAL, "Commercial", 100, "Commercial: $100"},
    {TOOL_INDUSTRIAL, "Industrial", 100, "Industrial: $100"},
    {TOOL_POLICESTATION, "Police", 500, "Police: $500"},
    {TOOL_FIRESTATION, "Fire dept", 500, "Fire dept: $500"},
    {TOOL_STADIUM, "Stadium", 5000, "Stadium: $5,000"},
    {TOOL_SEAPORT, "Seaport", 3000, "Seaport: $3,000"},
    {TOOL_COALPOWER, "Coal power", 3000, "Coal power: $3,000"},
    {TOOL_NUCLEARPOWER, "Nuclear power", 5000, "Nuclear power: $5,000"},
    {TOOL_AIRPORT, "Airport", 10000, "Airport: $10,000"},
    {TOOL_QUERY, "Query", 0, "Query"},
};

const int kToolCount = static_cast<int>(sizeof(kTools) / sizeof(kTools[0]));
const int kDefaultToolIndex = 3; // Power lines

const ToolDef *tool_by_index(int index)
{
    if (index < 0 || index >= kToolCount) {
        return &kTools[0];
    }
    return &kTools[index];
}

ToolFootprint tool_footprint(int engine_id)
{
    // Same sizes as static gToolSize[] in the engine (indexed by EditingTool):
    //   res/com/ind/fire/police = 3, stadium/seaport/coal/nuclear = 4,
    //   airport = 6, query and the line tools = 1.
    // Multi-tile buildings are anchored on the cursor, then buildBuilding()
    // moves to the top-left with mapH--; mapV--.
    switch (engine_id) {
    case TOOL_RESIDENTIAL:
    case TOOL_COMMERCIAL:
    case TOOL_INDUSTRIAL:
    case TOOL_FIRESTATION:
    case TOOL_POLICESTATION:
        return {true, 3, 3, 1, 1};
    case TOOL_STADIUM:
    case TOOL_SEAPORT:
    case TOOL_COALPOWER:
    case TOOL_NUCLEARPOWER:
        return {true, 4, 4, 1, 1};
    case TOOL_AIRPORT:
        return {true, 6, 6, 1, 1};
    case TOOL_WIRE:
    case TOOL_BULLDOZER:
    case TOOL_RAILROAD:
    case TOOL_ROAD:
    case TOOL_PARK:
    case TOOL_NETWORK:
    case TOOL_WATER:
    case TOOL_LAND:
    case TOOL_FOREST:
        return {true, 1, 1, 0, 0};
    case TOOL_QUERY:
    default:
        return {false, 1, 1, 0, 0};
    }
}

namespace {

struct ToolIdCheck {
    ToolIdCheck()
    {
        assert(TOOL_BULLDOZER == kTools[0].engine_id);
        assert(TOOL_WIRE == kTools[3].engine_id);
        assert(TOOL_QUERY == kTools[15].engine_id);
        assert(kToolCount == 16);
    }
};

const ToolIdCheck kToolIdCheck;

} // namespace

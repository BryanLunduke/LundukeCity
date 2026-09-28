// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// Palette order matches the classic left-hand tool column (top to bottom,
// left to right). engine_id values are the Micropolis EditingTool ordinals.
struct ToolDef {
    int engine_id;
    const char *name;
    int cost;
    const char *hint;
};

extern const ToolDef kTools[];
extern const int kToolCount;
extern const int kDefaultToolIndex;

const ToolDef *tool_by_index(int index);

// Footprint of a placeable tool, in tiles. width/height follow the engine's
// gToolSize[] (tool.cpp). Buildings larger than one tile use the cursor as
// the center; buildBuilding() then steps one tile up and left (mapH--; mapV--),
// which is cursor_to_left / cursor_to_top. Query is not placeable.
struct ToolFootprint {
    bool placeable = false;
    int width = 1;
    int height = 1;
    int cursor_to_left = 0;
    int cursor_to_top = 0;
};

ToolFootprint tool_footprint(int engine_id);

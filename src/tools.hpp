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

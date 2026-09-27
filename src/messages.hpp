// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <string>

// Short notices for Micropolis message numbers (text.h MessageNumber).
// Wording is original to Lunduke City; the engine itself only sends numbers.
std::string message_for_number(int number);

// One-line query readout. tile_category is the 1-based index from
// doShowZoneStatus (29 means clear land; the engine's dirt path reports that).
std::string zone_status_text(int tile_category, int population, int land_value,
                             int crime, int pollution, int growth);

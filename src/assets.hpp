// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <string>

// Directory that contains images/ and res/ from the vendored Micropolis assets.
// Empty if the tree cannot be found.
std::string asset_root();

// asset_root() + "/" + relative, or empty if the root or file is missing.
std::string asset_file(const std::string &relative);

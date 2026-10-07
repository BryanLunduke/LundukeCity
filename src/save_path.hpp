// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cctype>
#include <string>

// True when the path already ends in .cty, ignoring case.
inline bool path_has_cty_suffix(const std::string &path)
{
    if (path.size() < 4) {
        return false;
    }
    const unsigned char dot = static_cast<unsigned char>(path[path.size() - 4]);
    const unsigned char a = static_cast<unsigned char>(path[path.size() - 3]);
    const unsigned char b = static_cast<unsigned char>(path[path.size() - 2]);
    const unsigned char c = static_cast<unsigned char>(path[path.size() - 1]);
    return dot == '.' && std::tolower(a) == 'c' && std::tolower(b) == 't' && std::tolower(c) == 'y';
}

// The path that will actually be opened. Append .cty before the chooser
// confirms a name that does not already have the suffix.
inline std::string with_cty_suffix(const std::string &path)
{
    if (path_has_cty_suffix(path)) {
        return path;
    }
    return path + ".cty";
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// Packaging identity for the LCOS 0.9 track. The Debian revision is 0.9-10.
// The in-app version string is the release track, 0.9.
// The Meson project version is the dotted form, 0.9.0.
#ifndef LUNDUKE_CITY_PACKAGE_VERSION
#define LUNDUKE_CITY_PACKAGE_VERSION "0.9-10"
#endif

inline constexpr const char kPackageVersion[] = LUNDUKE_CITY_PACKAGE_VERSION;
inline constexpr const char kReleaseTrack[] = "0.9";

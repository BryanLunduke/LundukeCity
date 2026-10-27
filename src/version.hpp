// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// Packaging identity for LCOS 1.0.0. The Debian package version is 1.0.0-1.
// The in-app version string is 1.0.0.
// The Meson project version is 1.0.0.
#ifndef LUNDUKE_CITY_PACKAGE_VERSION
#define LUNDUKE_CITY_PACKAGE_VERSION "1.0.0-1"
#endif

inline constexpr const char kPackageVersion[] = LUNDUKE_CITY_PACKAGE_VERSION;
inline constexpr const char kReleaseTrack[] = "1.0.0";

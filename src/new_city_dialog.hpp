// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "city_session.hpp"

#include <gtkmm/window.h>

#include <string>

// Multi-step new-city wizard: name, then difficulty / terrain / seed, then
// generate. Returns false when the user cancels. shot_path, when set, opens
// the terrain page, writes a PNG, and cancels without generating.
bool run_new_city_wizard(Gtk::Window &parent, CitySession::NewCitySpec &spec,
                         const std::string &shot_path);

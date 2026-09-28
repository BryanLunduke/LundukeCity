// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/dialog.h>

class AboutDialog : public Gtk::Dialog {
public:
    explicit AboutDialog(Gtk::Window &parent);
};

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <gtkmm/drawingarea.h>

// Two-by-eight tool grid. Icons are original drawings, not imported artwork.
class ToolPalette : public Gtk::DrawingArea {
public:
    ToolPalette();

    int selected() const { return selected_; }
    void set_selected(int index);

    sigc::signal<void, int> signal_selected;

protected:
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;

private:
    int index_at(double x, double y) const;

    static constexpr int kCols = 2;
    static constexpr int kCell = 34;
    static constexpr int kPad = 4;

    int selected_ = 0;
};

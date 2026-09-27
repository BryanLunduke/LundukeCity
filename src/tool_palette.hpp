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
    void on_realize() override;
    bool on_draw(const Cairo::RefPtr<Cairo::Context> &cr) override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;
    bool on_leave_notify_event(GdkEventCrossing *event) override;

private:
    bool on_query_tooltip(int x, int y, bool keyboard_tooltip,
                          const Glib::RefPtr<Gtk::Tooltip> &tooltip);
    int index_at(double x, double y) const;

    static constexpr int kCols = 2;
    static constexpr int kCell = 36;
    static constexpr int kPad = 4;

    int selected_ = 0;
    int hover_ = -1;
};

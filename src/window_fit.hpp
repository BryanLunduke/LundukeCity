#pragma once
// Keep a top-level window's frame inside the monitor work area (r13 C-2).
#include <gtkmm.h>
#include <algorithm>

namespace window_fit {

// Frame allowance: title bar above the client area plus the bottom border.
constexpr int kFrameTop = 40;
constexpr int kFrameBottom = 8;
constexpr int kFrameSide = 8;

// Clamp a frame origin (x, y) for a client of size w x h into the area.
inline void clamp_origin(int &x, int &y, int w, int h, int ax, int ay, int aw, int ah)
{
    const int max_x = ax + aw - w - 2 * kFrameSide;
    const int max_y = ay + ah - h - kFrameTop - kFrameBottom;
    x = std::max(ax, std::min(x, max_x));
    y = std::max(ay, std::min(y, max_y));
}

inline bool workarea_for(Gtk::Window &win, Gdk::Rectangle &area)
{
    auto display = Gdk::Display::get_default();
    if (!display) {
        return false;
    }
    Glib::RefPtr<Gdk::Monitor> monitor;
    if (auto gw = win.get_window()) {
        monitor = display->get_monitor_at_window(gw);
    }
    if (!monitor) {
        monitor = display->get_primary_monitor();
    }
    if (!monitor && display->get_n_monitors() > 0) {
        monitor = display->get_monitor(0);
    }
    if (!monitor) {
        return false;
    }
    monitor->get_workarea(area);
    return true;
}

// Move the window back inside the work area if its frame runs past an edge.
inline bool keep_on_screen(Gtk::Window &win)
{
    Gdk::Rectangle area;
    if (!workarea_for(win, area)) {
        return false;
    }
    int x = 0;
    int y = 0;
    win.get_position(x, y);
    int w = win.get_width();
    int h = win.get_height();
    int nx = x;
    int ny = y;
    clamp_origin(nx, ny, w, h, area.get_x(), area.get_y(), area.get_width(), area.get_height());
    if (nx != x || ny != y) {
        win.move(nx, ny);
        return true;
    }
    return false;
}

// Clamp now and again once the window manager has placed the window.
inline void keep_on_screen_after_map(Gtk::Window &win)
{
    // track_obj: the timeouts are dropped if the window is destroyed first.
    win.signal_map().connect([&win]() {
        Glib::signal_timeout().connect_once(sigc::track_obj([&win]() { keep_on_screen(win); }, win), 50);
        Glib::signal_timeout().connect_once(sigc::track_obj([&win]() { keep_on_screen(win); }, win), 400);
    });
}

} // namespace window_fit

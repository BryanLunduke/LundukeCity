// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include "budget_window.hpp"
#include "evaluation_window.hpp"
#include "graphs_window.hpp"
#include "map_view.hpp"
#include "overlay_window.hpp"
#include "side_widgets.hpp"
#include "sound_player.hpp"
#include "tool_palette.hpp"

#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/dialog.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/menubar.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/recentchoosermenu.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/sizegroup.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

class CitySession;

class AppWindow : public Gtk::ApplicationWindow {
public:
    AppWindow();
    ~AppWindow() override;

    enum class CityFileOpen {
        Loaded,
        Cancelled,
        Failed,
    };

    // Same path as Load City, including the unsaved-city question.
    // A missing or unreadable file shows an error dialog and does not
    // replace the city. The process stays open.
    CityFileOpen open_city_file(const std::string &path);

private:
    void build_ui();
    void build_menus();
    void bind_session();
    void refresh();
    void on_session_changed_minimized();
    void sync_welcome_clock();
    void sync_option_checks();
    void show_tool_hint();
    void clear_transient_message();
    void set_speed(int speed);
    void toggle_pause();
    bool confirm_disaster(const char *name, const char *question, const char *detail);
    bool refresh_needed();
    bool on_window_state_event(GdkEventWindowState *event) override;
    void center_on_fraction(double fx, double fy);
    void zoom_by(int delta);
    void show_query_dialog(const std::string &text);
    void grab_screenshot_if_requested();
    void probe_zoom_if_requested();

    void on_new_city();
    bool replace_with_loaded_city(const std::string &path);
    bool replace_with_scenario(int id);
    void on_load_city();
    CityFileOpen finish_open_city(const std::string &path);
    void on_save_city();
    void on_save_city_as();
    void report_save_failure();
    void present_scenario_outcome(int outcome);
    void on_play_scenario();
    void on_rename_city();
    void on_budget();
    void on_budget_hidden();
    void on_quit();
    bool confirm_unsaved();
    void close_budget_window();
    void on_graphs();
    void on_evaluation();
    void on_overlay(CitySession::MapLayer layer);
    void on_about();
    void begin_modal();
    void end_modal();
    void release_query_pin();
    void prepare_demo_if_requested();
    void save_widget_png(Gtk::Widget &widget, const char *path);
    void grab_followup_shots();

    bool on_tick();
    // C-1 (round 15): autosave and crash recovery.
    bool on_autosave_timer();
    bool autosave_now(bool force);
    void drop_own_recovery();
    void offer_recovery();
    static gboolean on_terminate_signal(gpointer self);
    // P-4 (round 15, City too): desktop recent files + System > Open Recent.
    void remember_recent_city(const std::string &path);
    void on_recent_city_activated();
    bool on_key_press_event(GdkEventKey *event) override;
    bool on_delete_event(GdkEventAny *event) override;

    std::unique_ptr<CitySession> session_;

    Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
    Gtk::MenuBar menu_bar_;
    Gtk::EventBox status_events_;
    Gtk::Box status_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Label funds_label_;
    Gtk::Label name_label_;
    Gtk::Label years_label_;
    Gtk::Label date_label_;
    Gtk::Box welcome_bar_{Gtk::ORIENTATION_HORIZONTAL, 8};
    Gtk::Label welcome_label_;
    Gtk::Button welcome_dismiss_{"_Dismiss", true};
    Gtk::Box body_{Gtk::ORIENTATION_HORIZONTAL, 0};
    Gtk::ScrolledWindow side_scroll_;
    Gtk::EventBox side_events_;
    Gtk::Box side_{Gtk::ORIENTATION_VERTICAL, 4};
    ToolPalette tools_;
    MinimapView minimap_;
    DemandView demand_;
    Gtk::Frame map_frame_;
    Gtk::ScrolledWindow scroll_;
    MapView map_;
    Gtk::EventBox message_events_;
    Gtk::Box message_bar_{Gtk::ORIENTATION_VERTICAL, 2};
    Gtk::Label message_label_;
    Gtk::Label goal_label_;

    Gtk::CheckMenuItem *auto_budget_item_ = nullptr;
    Gtk::CheckMenuItem *auto_bulldoze_item_ = nullptr;
    Gtk::CheckMenuItem *disasters_item_ = nullptr;
    Gtk::CheckMenuItem *auto_goto_item_ = nullptr;
    Gtk::CheckMenuItem *mute_item_ = nullptr;
    Gtk::RadioMenuItem *speed_items_[4] = {nullptr, nullptr, nullptr, nullptr};

    BudgetWindow budget_window_;
    GraphsWindow graphs_window_;
    EvaluationWindow evaluation_window_;
    std::unique_ptr<OverlayWindow> overlays_[6];
    SoundPlayer sound_;

    Glib::RefPtr<Gtk::AccelGroup> accel_;
    Glib::RefPtr<Gtk::SizeGroup> status_ends_;
    sigc::connection timer_;
    sigc::connection autosave_timer_;
    std::string own_recovery_;
    std::string autosave_fingerprint_;
    unsigned sigterm_source_ = 0;
    unsigned sighup_source_ = 0;
    bool terminating_ = false;
    Gtk::RecentChooserMenu *recent_menu_ = nullptr;
    bool updating_checks_ = false;
    int speed_ = 0;
    std::string tool_hint_ = "Power lines: $5. Underwater wire: $25";
    std::string shown_engine_message_;
    int shown_message_serial_ = 0;
    bool scenario_dialog_open_ = false;
    std::chrono::steady_clock::time_point hint_after_{};
    std::unique_ptr<Gtk::Dialog> query_dialog_;
    Gtk::Label *query_body_ = nullptr;
    int shown_query_serial_ = 0;
    bool query_pinned_ = false;
    int modal_depth_ = 0;
    bool tax_budget_modal_ = false;
    int paused_from_speed_ = 2;

    // What the last refresh() showed. A paused city skips refresh() until
    // one of these changes (C-2). A minimized window skips it entirely.
    struct ShownState {
        unsigned map_serial = 0;
        long long city_time = -1;
        std::string funds;
        std::string name;
        int message_serial = -1;
        int query_serial = -1;
        int speed = -1;
        bool sim_paused = false;
        bool notice_expired = false;
        bool budget_pending = false;
        int years_left = -2;
        bool operator==(const ShownState &o) const
        {
            return map_serial == o.map_serial && city_time == o.city_time && funds == o.funds &&
                   name == o.name && message_serial == o.message_serial &&
                   query_serial == o.query_serial && speed == o.speed && sim_paused == o.sim_paused &&
                   notice_expired == o.notice_expired && budget_pending == o.budget_pending &&
                   years_left == o.years_left;
        }
    };
    ShownState current_shown_state() const;
    ShownState shown_state_;
    bool iconified_ = false;
    std::string shown_title_;
    unsigned long refresh_count_ = 0;

    class ModalPause {
    public:
        explicit ModalPause(AppWindow &window) : window_(window) { window_.begin_modal(); }
        ~ModalPause() { window_.end_modal(); }
        ModalPause(const ModalPause &) = delete;
        ModalPause &operator=(const ModalPause &) = delete;

    private:
        AppWindow &window_;
    };
    int quake_strength_ = 0;
    std::chrono::steady_clock::time_point quake_started_{};
    std::chrono::steady_clock::time_point quake_until_{};

#ifdef LUNDUKE_CITY_TEST_HOOKS
    friend int hostile_review_window_probe(AppWindow &window, int op);
    friend int round8_window_probe(AppWindow &window, int op);
    friend int round9_window_probe(AppWindow &window, int op);
    friend int round12_window_probe(AppWindow &window, int op);
    friend int round15_window_probe(AppWindow &window, int op, const std::string &arg);
#endif
};

#ifdef LUNDUKE_CITY_TEST_HOOKS
// Drives load, scenario, and announcement-pause paths without a file dialog.
int hostile_review_window_probe(AppWindow &window, int op);
int round8_window_probe(AppWindow &window, int op);
// Round 9: minimized tax-year pause and dropped sounds (C-1).
int round9_window_probe(AppWindow &window, int op);
// Round 12: Help menu, Save As shortcut, empty-city evaluation, load
// reasons, full map redraw, Budget fits the work area. Returns a bit mask.
int round12_window_probe(AppWindow &window, int op);
// Round 15: autosave, recovery prompt, SIGTERM, Open Recent.
int round15_window_probe(AppWindow &window, int op, const std::string &arg);
#endif

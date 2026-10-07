// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "app_window.hpp"

#include "about_dialog.hpp"
#include "city_session.hpp"
#include "new_city_dialog.hpp"
#include "tools.hpp"
#include "zoom_keys.hpp"

#include "micropolis.h"

#include <gdkmm/pixbuf.h>
#include <glibmm/main.h>
#include <gtk/gtk.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/liststore.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/treeview.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>

static_assert(static_cast<unsigned>(GDK_KEY_equal) == 0x03d, "equal keysym");
static_assert(static_cast<unsigned>(GDK_KEY_plus) == 0x02b, "plus keysym");
static_assert(static_cast<unsigned>(GDK_KEY_KP_Add) == 0xffab, "keypad plus");
static_assert(static_cast<unsigned>(GDK_KEY_minus) == 0x02d, "minus keysym");
static_assert(static_cast<unsigned>(GDK_KEY_KP_Subtract) == 0xffad, "keypad minus");
static_assert(static_cast<unsigned>(Gdk::CONTROL_MASK) == 4u, "control mask");
static_assert(static_cast<unsigned>(Gdk::MOD1_MASK) == 8u, "alt mask");

AppWindow::~AppWindow() = default;

AppWindow::AppWindow()
{
    set_title("Lunduke City");
    // Reinforce default icon for WMs that ignore gtk_window_set_default_icon_name.
    set_icon_name("lunduke-city");
    set_default_size(1100, 740);
    session_ = std::make_unique<CitySession>();
    const CitySession::MapLayer layers[] = {
        CitySession::MapLayer::Power,    CitySession::MapLayer::Water,
        CitySession::MapLayer::Pollution, CitySession::MapLayer::Crime,
        CitySession::MapLayer::LandValue, CitySession::MapLayer::Traffic,
    };
    for (int i = 0; i < 6; ++i) {
        overlays_[i] = std::make_unique<OverlayWindow>(layers[i]);
        overlays_[i]->set_session(session_.get());
    }
    budget_window_.set_session(session_.get());
    graphs_window_.set_session(session_.get());
    evaluation_window_.set_session(session_.get());
    build_ui();
    build_menus();
    bind_session();
    add(root_);
    show_all_children();

    session_->set_listener([this] { refresh(); });
    session_->new_city("New City");
    session_->set_speed(speed_);
    sync_option_checks();
    show_tool_hint();
    refresh();

    timer_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &AppWindow::on_tick), 100);
    Glib::signal_timeout().connect_once([this] { center_on_fraction(0.5, 0.5); }, 200);
    Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &AppWindow::grab_screenshot_if_requested),
                                        600);
}

void AppWindow::build_ui()
{
    accel_ = Gtk::AccelGroup::create();
    add_accel_group(accel_);

    status_.set_border_width(5);
    status_.set_margin_start(6);
    status_.set_margin_end(6);
    status_ends_ = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);
    funds_label_.set_halign(Gtk::ALIGN_START);
    funds_label_.set_xalign(0);
    name_label_.set_hexpand(true);
    name_label_.set_halign(Gtk::ALIGN_CENTER);
    name_label_.set_xalign(0.5);
    date_label_.set_halign(Gtk::ALIGN_END);
    date_label_.set_xalign(1);
    status_ends_->add_widget(funds_label_);
    status_ends_->add_widget(date_label_);
    status_.pack_start(funds_label_, Gtk::PACK_SHRINK);
    status_.pack_start(name_label_, Gtk::PACK_EXPAND_WIDGET);
    status_.pack_start(date_label_, Gtk::PACK_SHRINK);
    status_events_.add(status_);
    status_events_.set_name("status-bar");

    side_.set_border_width(6);
    side_.set_spacing(6);
    side_.set_hexpand(false);
    side_.set_halign(Gtk::ALIGN_START);
    tools_.set_hexpand(false);
    tools_.set_halign(Gtk::ALIGN_START);
    tools_.set_valign(Gtk::ALIGN_START);
    side_.pack_start(tools_, Gtk::PACK_SHRINK);
    side_.pack_start(minimap_, Gtk::PACK_SHRINK);
    side_.pack_start(demand_, Gtk::PACK_SHRINK);
    side_events_.add(side_);
    side_events_.set_name("side-panel");
    side_events_.set_hexpand(false);
    side_events_.set_halign(Gtk::ALIGN_START);
    side_events_.set_valign(Gtk::ALIGN_FILL);
    map_frame_.set_hexpand(true);

    scroll_.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scroll_.set_kinetic_scrolling(false);
    scroll_.set_overlay_scrolling(false);
    scroll_.add(map_);
    map_frame_.add(scroll_);
    map_frame_.set_name("map-frame");
    map_frame_.set_shadow_type(Gtk::SHADOW_IN);

    message_label_.set_halign(Gtk::ALIGN_START);
    message_label_.set_xalign(0);
    message_label_.set_hexpand(true);
    message_label_.set_margin_start(8);
    message_bar_.set_border_width(5);
    message_bar_.pack_start(message_label_, Gtk::PACK_EXPAND_WIDGET);
    message_events_.add(message_bar_);
    message_events_.set_name("message-bar");

    body_.pack_start(side_events_, Gtk::PACK_SHRINK);
    body_.pack_start(map_frame_, Gtk::PACK_EXPAND_WIDGET);

    root_.pack_start(menu_bar_, Gtk::PACK_SHRINK);
    root_.pack_start(status_events_, Gtk::PACK_SHRINK);
    root_.pack_start(body_, Gtk::PACK_EXPAND_WIDGET);
    root_.pack_start(message_events_, Gtk::PACK_SHRINK);
}

void AppWindow::build_menus()
{
    auto add_item = [this](Gtk::Menu *menu, const Glib::ustring &label, guint key,
                           const sigc::slot<void> &slot) {
        auto *item = Gtk::manage(new Gtk::MenuItem(label, true));
        item->signal_activate().connect(slot);
        if (key != 0) {
            item->add_accelerator("activate", accel_, key, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
        }
        menu->append(*item);
        return item;
    };

    auto *system_menu = Gtk::manage(new Gtk::Menu());
    auto *system = Gtk::manage(new Gtk::MenuItem("_System", true));
    system->set_submenu(*system_menu);
    add_item(system_menu, "_New City", GDK_KEY_n, sigc::mem_fun(*this, &AppWindow::on_new_city));
    add_item(system_menu, "_Load City...", GDK_KEY_o, sigc::mem_fun(*this, &AppWindow::on_load_city));
    add_item(system_menu, "_Save City", GDK_KEY_s, sigc::mem_fun(*this, &AppWindow::on_save_city));
    add_item(system_menu, "Save City _As...", 0, sigc::mem_fun(*this, &AppWindow::on_save_city_as));
    add_item(system_menu, "Play _Scenario…", 0, sigc::mem_fun(*this, &AppWindow::on_play_scenario));
    add_item(system_menu, "_Rename City…", 0, sigc::mem_fun(*this, &AppWindow::on_rename_city));
    system_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    add_item(system_menu, "_Quit", GDK_KEY_q, [this] { hide(); });
    menu_bar_.append(*system);

    auto *options_menu = Gtk::manage(new Gtk::Menu());
    auto *options = Gtk::manage(new Gtk::MenuItem("_Options", true));
    options->set_submenu(*options_menu);

    auto_budget_item_ = Gtk::manage(new Gtk::CheckMenuItem("Auto _budget", true));
    auto_bulldoze_item_ = Gtk::manage(new Gtk::CheckMenuItem("Auto _bulldoze", true));
    disasters_item_ = Gtk::manage(new Gtk::CheckMenuItem("Enable _disasters", true));
    auto_goto_item_ = Gtk::manage(new Gtk::CheckMenuItem("Auto-_goto", true));
    mute_item_ = Gtk::manage(new Gtk::CheckMenuItem("_Mute sound", true));
    options_menu->append(*auto_budget_item_);
    options_menu->append(*auto_bulldoze_item_);
    options_menu->append(*disasters_item_);
    options_menu->append(*auto_goto_item_);
    options_menu->append(*mute_item_);
    options_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));

    Gtk::RadioMenuItem::Group speed_group;
    const char *speed_labels[] = {"Pause", "Slow", "Medium", "Fast"};
    for (int i = 0; i < 4; ++i) {
        speed_items_[i] = Gtk::manage(new Gtk::RadioMenuItem(speed_group, speed_labels[i]));
        options_menu->append(*speed_items_[i]);
    }
    options_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    // Zoom in: the visible shortcut is Ctrl and the +/= key with no Shift
    // (GDK_KEY_equal). Also keep the shifted plus keysym and keypad plus.
    // Zoom out stays on Ctrl-minus and keypad minus.
    auto *zoom_in = Gtk::manage(new Gtk::MenuItem("Zoom _in", true));
    zoom_in->signal_activate().connect([this] { zoom_by(2); });
    // The menu shows Ctrl+= (the +/= key, no Shift). Keypad plus and the
    // shifted plus keysym stay wired, without replacing that label.
    const auto hidden = static_cast<Gtk::AccelFlags>(0);
    zoom_in->add_accelerator("activate", accel_, GDK_KEY_equal, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    zoom_in->add_accelerator("activate", accel_, GDK_KEY_plus, Gdk::CONTROL_MASK, hidden);
    zoom_in->add_accelerator("activate", accel_, GDK_KEY_KP_Add, Gdk::CONTROL_MASK, hidden);
    options_menu->append(*zoom_in);
    auto *zoom_out = Gtk::manage(new Gtk::MenuItem("Zoom _out", true));
    zoom_out->signal_activate().connect([this] { zoom_by(-2); });
    zoom_out->add_accelerator("activate", accel_, GDK_KEY_minus, Gdk::CONTROL_MASK, Gtk::ACCEL_VISIBLE);
    zoom_out->add_accelerator("activate", accel_, GDK_KEY_KP_Subtract, Gdk::CONTROL_MASK, hidden);
    options_menu->append(*zoom_out);
    menu_bar_.append(*options);

    auto *disasters_menu = Gtk::manage(new Gtk::Menu());
    auto *disasters = Gtk::manage(new Gtk::MenuItem("_Disasters", true));
    disasters->set_submenu(*disasters_menu);
    add_item(disasters_menu, "_Fire", 0, [this] { session_->disaster_fire(); });
    add_item(disasters_menu, "F_lood", 0, [this] { session_->disaster_flood(); });
    add_item(disasters_menu, "_Tornado", 0, [this] { session_->disaster_tornado(); });
    add_item(disasters_menu, "_Earthquake", 0, [this] { session_->disaster_earthquake(); });
    add_item(disasters_menu, "_Monster", 0, [this] { session_->disaster_monster(); });
    add_item(disasters_menu, "_Meltdown", 0, [this] { session_->disaster_meltdown(); });
    menu_bar_.append(*disasters);

    auto *windows_menu = Gtk::manage(new Gtk::Menu());
    auto *windows = Gtk::manage(new Gtk::MenuItem("_Windows", true));
    windows->set_submenu(*windows_menu);
    add_item(windows_menu, "_Budget", 0, sigc::mem_fun(*this, &AppWindow::on_budget));
    add_item(windows_menu, "_Graphs", 0, sigc::mem_fun(*this, &AppWindow::on_graphs));
    add_item(windows_menu, "_Evaluation", 0, sigc::mem_fun(*this, &AppWindow::on_evaluation));
    windows_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    add_item(windows_menu, "_Power", 0, [this] { on_overlay(CitySession::MapLayer::Power); });
    add_item(windows_menu, "_Water", 0, [this] { on_overlay(CitySession::MapLayer::Water); });
    add_item(windows_menu, "P_ollution", 0, [this] { on_overlay(CitySession::MapLayer::Pollution); });
    add_item(windows_menu, "_Crime", 0, [this] { on_overlay(CitySession::MapLayer::Crime); });
    add_item(windows_menu, "_Land value", 0, [this] { on_overlay(CitySession::MapLayer::LandValue); });
    add_item(windows_menu, "_Traffic", 0, [this] { on_overlay(CitySession::MapLayer::Traffic); });
    windows_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    add_item(windows_menu, "_About Lunduke City", 0, sigc::mem_fun(*this, &AppWindow::on_about));
    menu_bar_.append(*windows);
}

void AppWindow::bind_session()
{
    map_.set_session(session_.get());
    minimap_.set_session(session_.get());
    demand_.set_session(session_.get());
    tools_.set_selected(kDefaultToolIndex);
    map_.set_tool(tool_by_index(kDefaultToolIndex)->engine_id);
    tool_hint_ = tool_by_index(kDefaultToolIndex)->hint;

    tools_.signal_selected.connect([this](int index) {
        const ToolDef *tool = tool_by_index(index);
        map_.set_tool(tool->engine_id);
        tool_hint_ = tool->hint;
        // A freshly chosen tool should show its price even if the engine
        // has a standing notice. The next new notice can replace it briefly.
        shown_engine_message_ = session_->message();
        hint_after_ = std::chrono::steady_clock::time_point{};
        show_tool_hint();
    });

    map_.signal_tool_down.connect([this](int x, int y) {
        session_->use_tool(tool_by_index(tools_.selected())->engine_id, x, y);
        refresh();
    });
    map_.signal_zoom.connect(sigc::mem_fun(*this, &AppWindow::zoom_by));
    map_.signal_tool_drag.connect([this](int x0, int y0, int x1, int y1) {
        session_->drag_tool(tool_by_index(tools_.selected())->engine_id, x0, y0, x1, y1);
        refresh();
    });

    minimap_.set_viewport_provider([this](double &x, double &y, double &w, double &h) {
        auto ha = scroll_.get_hadjustment();
        auto va = scroll_.get_vadjustment();
        const double mw = std::max(1, map_.pixel_width());
        const double mh = std::max(1, map_.pixel_height());
        x = ha->get_value() / mw;
        y = va->get_value() / mh;
        w = ha->get_page_size() / mw;
        h = va->get_page_size() / mh;
    });
    minimap_.signal_jump.connect(sigc::mem_fun(*this, &AppWindow::center_on_fraction));

    auto_budget_item_->signal_toggled().connect([this] {
        if (!updating_checks_) {
            session_->set_auto_budget(auto_budget_item_->get_active());
        }
    });
    auto_bulldoze_item_->signal_toggled().connect([this] {
        if (!updating_checks_) {
            session_->set_auto_bulldoze(auto_bulldoze_item_->get_active());
        }
    });
    disasters_item_->signal_toggled().connect([this] {
        if (!updating_checks_) {
            session_->set_disasters(disasters_item_->get_active());
        }
    });
    auto_goto_item_->signal_toggled().connect([this] {
        if (!updating_checks_) {
            session_->set_auto_goto(auto_goto_item_->get_active());
        }
    });
    mute_item_->signal_toggled().connect([this] {
        if (!updating_checks_) {
            const bool enabled = !mute_item_->get_active();
            session_->set_sound_enabled(enabled);
            sound_.set_muted(!enabled);
        }
    });
    for (int i = 0; i < 4; ++i) {
        speed_items_[i]->signal_toggled().connect([this, i] {
            if (!updating_checks_ && speed_items_[i]->get_active()) {
                set_speed(i);
            }
        });
    }

    scroll_.get_hadjustment()->signal_value_changed().connect([this] { minimap_.invalidate_viewport(); });
    scroll_.get_vadjustment()->signal_value_changed().connect([this] { minimap_.invalidate_viewport(); });
}

void AppWindow::refresh()
{
    funds_label_.set_text(session_->funds_text());
    name_label_.set_text(session_->city_name());
    date_label_.set_text(session_->date_text());
    set_title("Lunduke City - " + session_->city_name());

    const auto now = std::chrono::steady_clock::now();
    const std::string engine_message = session_->message();
    if (!engine_message.empty() && engine_message != shown_engine_message_) {
        shown_engine_message_ = engine_message;
        message_label_.set_text(engine_message);
        hint_after_ = now + std::chrono::seconds(4);
        query_pinned_ = false;
    } else if (hint_after_.time_since_epoch().count() == 0 || now >= hint_after_) {
        message_label_.set_text(tool_hint_);
    }

    // Query used to vanish into the tool hint. Keep a dialog open with the
    // zone report, and pin that report on the message bar.
    if (session_->query_serial() != shown_query_serial_) {
        shown_query_serial_ = session_->query_serial();
        if (!engine_message.empty()) {
            message_label_.set_text(engine_message);
            shown_engine_message_ = engine_message;
            hint_after_ = now + std::chrono::hours(1);
            query_pinned_ = true;
            show_query_dialog(engine_message);
        }
    }

    int goto_x = 0;
    int goto_y = 0;
    if (session_->take_view_target(goto_x, goto_y)) {
        center_on_fraction((goto_x + 0.5) / static_cast<double>(CitySession::kWorldW),
                           (goto_y + 0.5) / static_cast<double>(CitySession::kWorldH));
    }
    if (const int strength = session_->take_earthquake()) {
        quake_strength_ = strength;
        quake_started_ = now;
        const int milliseconds = std::max(400, std::min(strength, 1200));
        quake_until_ = now + std::chrono::milliseconds(milliseconds);
    }
    if (quake_strength_ > 0 && now < quake_until_) {
        const int elapsed = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - quake_started_).count());
        const int amp = std::max(2, quake_strength_ / 80);
        const int step = elapsed / 50;
        const int dx = (step % 2 == 0) ? amp : -amp;
        const int dy = ((step / 2) % 2 == 0) ? (amp / 2) : -(amp / 2);
        map_.set_shake(dx, dy);
    } else if (quake_strength_ > 0) {
        quake_strength_ = 0;
        map_.set_shake(0, 0);
    }

    map_.sync();
    minimap_.sync();
    demand_.queue_draw();
    if (budget_window_.get_visible()) {
        budget_window_.sync();
    }
    if (graphs_window_.get_visible()) {
        graphs_window_.sync();
    }
    if (evaluation_window_.get_visible()) {
        evaluation_window_.note_month(session_->game_month_index());
        evaluation_window_.sync();
    }
    for (auto &overlay : overlays_) {
        if (overlay && overlay->get_visible()) {
            overlay->sync();
        }
    }
    sound_.set_muted(!session_->sound_enabled());
    for (const auto &name : session_->take_sounds()) {
        sound_.play(name);
    }
    if (session_->take_budget_request()) {
        Glib::signal_idle().connect_once(sigc::mem_fun(*this, &AppWindow::on_budget));
    }
}

void AppWindow::sync_option_checks()
{
    updating_checks_ = true;
    auto_budget_item_->set_active(session_->auto_budget());
    auto_bulldoze_item_->set_active(session_->auto_bulldoze());
    disasters_item_->set_active(session_->disasters());
    auto_goto_item_->set_active(session_->auto_goto());
    mute_item_->set_active(!session_->sound_enabled());
    sound_.set_muted(!session_->sound_enabled());
    const int speed = session_->speed();
    if (speed >= 0 && speed < 4) {
        speed_items_[speed]->set_active(true);
    }
    updating_checks_ = false;
}

void AppWindow::show_tool_hint()
{
    message_label_.set_text(tool_hint_);
}

void AppWindow::clear_transient_message()
{
    shown_engine_message_.clear();
    hint_after_ = {};
    query_pinned_ = false;
    if (query_dialog_) {
        query_dialog_->hide();
    }
    show_tool_hint();
}

void AppWindow::begin_modal()
{
    if (modal_depth_++ == 0 && session_) {
        session_->set_simulation_paused(true);
    }
}

void AppWindow::end_modal()
{
    if (modal_depth_ == 0) {
        return;
    }
    if (--modal_depth_ == 0) {
        if (session_) {
            session_->set_simulation_paused(false);
        }
        refresh();
    }
}

void AppWindow::release_query_pin()
{
    if (!query_pinned_) {
        return;
    }
    query_pinned_ = false;
    hint_after_ = {};
    show_tool_hint();
}

void AppWindow::set_speed(int speed)
{
    speed_ = speed;
    session_->set_speed(speed);
}

void AppWindow::center_on_fraction(double fx, double fy)
{
    auto ha = scroll_.get_hadjustment();
    auto va = scroll_.get_vadjustment();
    const double x = fx * map_.pixel_width() - ha->get_page_size() / 2.0;
    const double y = fy * map_.pixel_height() - va->get_page_size() / 2.0;
    ha->set_value(std::max(ha->get_lower(), std::min(ha->get_upper() - ha->get_page_size(), x)));
    va->set_value(std::max(va->get_lower(), std::min(va->get_upper() - va->get_page_size(), y)));
}

void AppWindow::zoom_by(int delta)
{
    auto ha = scroll_.get_hadjustment();
    auto va = scroll_.get_vadjustment();
    const double old_w = std::max(1, map_.pixel_width());
    const double old_h = std::max(1, map_.pixel_height());
    // The point currently in the middle of the viewport stays there.
    const double cx = (ha->get_value() + ha->get_page_size() * 0.5) / old_w;
    const double cy = (va->get_value() + va->get_page_size() * 0.5) / old_h;
    const int before = map_.tile_size();
    map_.set_tile_size(before + delta);
    if (map_.tile_size() == before) {
        return;
    }
    // The scrolled window has not allocated the new child size yet.
    // Set the range now so the center is applied in the new pixel space.
    ha->set_upper(std::max(ha->get_page_size(), static_cast<double>(map_.pixel_width())));
    va->set_upper(std::max(va->get_page_size(), static_cast<double>(map_.pixel_height())));
    center_on_fraction(cx, cy);
}

void AppWindow::show_query_dialog(const std::string &text)
{
    if (!query_dialog_) {
        query_dialog_ = std::make_unique<Gtk::Dialog>("Query", *this, false);
        query_dialog_->set_transient_for(*this);
        query_dialog_->set_modal(false);
        query_dialog_->add_button("_Close", Gtk::RESPONSE_CLOSE);
        query_dialog_->set_default_size(380, 240);
        query_body_ = Gtk::manage(new Gtk::Label());
        query_body_->set_halign(Gtk::ALIGN_START);
        query_body_->set_valign(Gtk::ALIGN_START);
        query_body_->set_xalign(0);
        query_body_->set_yalign(0);
        query_body_->set_line_wrap(true);
        query_body_->set_max_width_chars(40);
        query_body_->set_selectable(true);
        auto *content = query_dialog_->get_content_area();
        content->set_border_width(12);
        content->set_spacing(6);
        content->pack_start(*query_body_, Gtk::PACK_EXPAND_WIDGET);
        query_dialog_->signal_response().connect([this](int) {
            query_dialog_->hide();
            release_query_pin();
        });
        query_dialog_->signal_delete_event().connect([this](GdkEventAny *) {
            query_dialog_->hide();
            release_query_pin();
            return true;
        });
    }

    std::string body;
    body.reserve(text.size() + 8);
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 3, " — ") == 0) {
            body.push_back('\n');
            i += 3;
            continue;
        }
        if (text.compare(i, 2, ", ") == 0) {
            body.push_back('\n');
            i += 2;
            continue;
        }
        body.push_back(text[i]);
        ++i;
    }
    query_body_->set_text(body);
    query_dialog_->show_all();
    query_dialog_->present();
}

bool AppWindow::on_tick()
{
    if (modal_depth_ > 0) {
        return true;
    }
    session_->tick();
    refresh();
    return true;
}

bool AppWindow::on_key_press_event(GdkEventKey *event)
{
    if (event != nullptr) {
        // Shift is not required. Ctrl and the +/= key arrives as GDK_KEY_equal.
        // Ctrl and the minus key arrives as GDK_KEY_minus.
        const ZoomAction action = zoom_action(event->keyval, event->state);
        if (action == ZoomAction::In) {
            zoom_by(2);
            return true;
        }
        if (action == ZoomAction::Out) {
            zoom_by(-2);
            return true;
        }
    }
    return Gtk::ApplicationWindow::on_key_press_event(event);
}

void AppWindow::on_new_city()
{
    ModalPause pause(*this);
    CitySession::NewCitySpec spec;
    const char *shot = std::getenv("LUNDUKE_CITY_SHOT_NEWCITY");
    const std::string shot_path = shot != nullptr ? shot : "";
    if (!run_new_city_wizard(*this, spec, shot_path)) {
        return;
    }
    session_->new_city(spec);
    session_->set_speed(speed_);
    sync_option_checks();
    clear_transient_message();
    refresh();
    center_on_fraction(0.5, 0.5);
}

void AppWindow::on_rename_city()
{
    ModalPause pause(*this);
    Gtk::Dialog dialog("Rename City", *this, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Rename", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    auto *content = dialog.get_content_area();
    auto *label = Gtk::manage(new Gtk::Label("New name for this city:"));
    label->set_halign(Gtk::ALIGN_START);
    auto *entry = Gtk::manage(new Gtk::Entry());
    entry->set_text(session_->city_name());
    entry->set_activates_default(true);
    content->set_border_width(12);
    content->set_spacing(6);
    content->pack_start(*label, Gtk::PACK_SHRINK);
    content->pack_start(*entry, Gtk::PACK_SHRINK);
    dialog.set_default_size(360, 120);
    dialog.show_all_children();

    const char *shot = std::getenv("LUNDUKE_CITY_SHOT_RENAME");
    if (shot != nullptr && shot[0] != '\0') {
        entry->set_text("Harbor Town");
        Glib::signal_timeout().connect_once(
            [&dialog, this, shot] {
                save_widget_png(dialog, shot);
                dialog.response(Gtk::RESPONSE_CANCEL);
            },
            400);
    }

    if (dialog.run() != Gtk::RESPONSE_OK) {
        return;
    }
    session_->rename_city(entry->get_text());
    refresh();
}

void AppWindow::on_load_city()
{
    ModalPause pause(*this);
    Gtk::FileChooserDialog dialog(*this, "Load City", Gtk::FILE_CHOOSER_ACTION_OPEN);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Open", Gtk::RESPONSE_ACCEPT);
    auto filter = Gtk::FileFilter::create();
    filter->set_name("City files");
    filter->add_pattern("*.cty");
    dialog.add_filter(filter);
    auto all = Gtk::FileFilter::create();
    all->set_name("All files");
    all->add_pattern("*");
    dialog.add_filter(all);
    if (dialog.run() != Gtk::RESPONSE_ACCEPT) {
        return;
    }
    if (!session_->load_city(dialog.get_filename())) {
        Gtk::MessageDialog error(*this, "Could not load that city file.", false, Gtk::MESSAGE_ERROR,
                                 Gtk::BUTTONS_OK, true);
        error.run();
        return;
    }
    session_->set_speed(speed_);
    sync_option_checks();
    clear_transient_message();
    refresh();
    center_on_fraction(0.5, 0.5);
}

void AppWindow::on_save_city()
{
    if (session_->save_path().empty()) {
        on_save_city_as();
        return;
    }
    session_->save_city_as(session_->save_path());
    refresh();
}

void AppWindow::on_save_city_as()
{
    ModalPause pause(*this);
    Gtk::FileChooserDialog dialog(*this, "Save City", Gtk::FILE_CHOOSER_ACTION_SAVE);
    dialog.set_do_overwrite_confirmation(true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Save", Gtk::RESPONSE_ACCEPT);
    auto filter = Gtk::FileFilter::create();
    filter->set_name("City files");
    filter->add_pattern("*.cty");
    dialog.add_filter(filter);
    dialog.set_current_name("city.cty");
    if (dialog.run() != Gtk::RESPONSE_ACCEPT) {
        return;
    }
    std::string path = dialog.get_filename();
    if (path.size() < 4 || path.substr(path.size() - 4) != ".cty") {
        path += ".cty";
    }
    if (!session_->save_city_as(path)) {
        Gtk::MessageDialog error(*this, "Could not save the city.", false, Gtk::MESSAGE_ERROR,
                                 Gtk::BUTTONS_OK, true);
        error.run();
    }
    refresh();
}

void AppWindow::on_play_scenario()
{
    ModalPause pause(*this);
    class Columns : public Gtk::TreeModel::ColumnRecord {
    public:
        Columns()
        {
            add(id);
            add(scenario);
            add(notes);
        }
        Gtk::TreeModelColumn<int> id;
        Gtk::TreeModelColumn<Glib::ustring> scenario;
        Gtk::TreeModelColumn<Glib::ustring> notes;
    };

    Columns columns;
    auto store = Gtk::ListStore::create(columns);
    for (int i = 0; i < CitySession::kScenarioCount; ++i) {
        const CitySession::ScenarioDef &def = CitySession::scenario_def(i);
        auto row = *store->append();
        row[columns.id] = def.id;
        row[columns.scenario] = Glib::ustring(def.name) + " (" + std::to_string(def.year) + ")";
        row[columns.notes] = def.summary;
    }

    Gtk::Dialog dialog("Play Scenario", *this, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Play", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    dialog.set_default_size(560, 420);

    auto *intro = Gtk::manage(new Gtk::Label(
        "Choose a scenario. Playing it replaces the city on the map."));
    intro->set_halign(Gtk::ALIGN_START);
    intro->set_line_wrap(true);
    intro->set_max_width_chars(52);

    auto *view = Gtk::manage(new Gtk::TreeView(store));
    view->append_column("Scenario", columns.scenario);
    view->append_column("Notes", columns.notes);
    view->set_headers_visible(true);
    if (auto *name_column = view->get_column(0)) {
        name_column->set_min_width(200);
    }
    if (auto *notes_column = view->get_column(1)) {
        notes_column->set_expand(true);
    }
    view->get_selection()->set_mode(Gtk::SELECTION_BROWSE);
    if (auto first = store->children().begin()) {
        view->get_selection()->select(first);
    }
    view->signal_row_activated().connect(
        [&dialog](const Gtk::TreeModel::Path &, Gtk::TreeViewColumn *) { dialog.response(Gtk::RESPONSE_OK); });

    auto *scroller = Gtk::manage(new Gtk::ScrolledWindow());
    scroller->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scroller->set_shadow_type(Gtk::SHADOW_IN);
    scroller->set_min_content_height(280);
    scroller->add(*view);

    auto *content = dialog.get_content_area();
    content->set_border_width(8);
    content->set_spacing(6);
    content->pack_start(*intro, Gtk::PACK_SHRINK);
    content->pack_start(*scroller, Gtk::PACK_EXPAND_WIDGET);
    dialog.show_all_children();

    const char *shot = std::getenv("LUNDUKE_CITY_SHOT_SCENARIO");
    if (shot != nullptr && shot[0] != '\0') {
        Glib::signal_timeout().connect_once(
            [&dialog, this, shot] {
                save_widget_png(dialog, shot);
                dialog.response(Gtk::RESPONSE_CANCEL);
            },
            400);
    }

    if (dialog.run() != Gtk::RESPONSE_OK) {
        return;
    }
    int id = -1;
    if (auto selected = view->get_selection()->get_selected()) {
        id = (*selected)[columns.id];
    }
    if (id < 0 || !session_->load_scenario(id)) {
        Gtk::MessageDialog error(*this, "Could not start that scenario.", false, Gtk::MESSAGE_ERROR,
                                 Gtk::BUTTONS_OK, true);
        error.run();
        return;
    }
    session_->set_speed(speed_);
    sync_option_checks();
    clear_transient_message();
    refresh();
    center_on_fraction(0.5, 0.5);
}

void AppWindow::on_budget()
{
    if (modal_depth_ > 0) {
        session_->keep_budget_request();
        return;
    }
    budget_window_.present_book();
}

void AppWindow::on_graphs()
{
    graphs_window_.present_graphs();
}

void AppWindow::on_overlay(CitySession::MapLayer layer)
{
    const int index = static_cast<int>(layer);
    if (index < 0 || index >= 6 || !overlays_[index]) {
        return;
    }
    overlays_[index]->present_map();
}

void AppWindow::on_evaluation()
{
    evaluation_window_.present_report();
}

void AppWindow::on_about()
{
    ModalPause pause(*this);
    AboutDialog dialog(*this);
    const char *shot = std::getenv("LUNDUKE_CITY_SHOT_ABOUT");
    if (shot != nullptr && shot[0] != '\0') {
        Glib::signal_timeout().connect_once(
            [&dialog, this, shot] {
                save_widget_png(dialog, shot);
                dialog.response(Gtk::RESPONSE_CLOSE);
            },
            400);
    }
    dialog.run();
}

void AppWindow::prepare_demo_if_requested()
{
    const char *demo = std::getenv("LUNDUKE_CITY_DEMO");
    if (demo == nullptr || demo[0] == '\0') {
        return;
    }
    int ox = 0;
    int oy = 0;
    session_->set_speed(3);
    if (!session_->stamp_neighborhood(ox, oy)) {
        session_->disaster_tornado();
        session_->tick();
        refresh();
        return;
    }
    // Enough simulator phases for a power scan to mark zones and wires.
    for (int i = 0; i < 180; ++i) {
        session_->tick();
    }
    session_->place_sprite(SPRITE_AIRPLANE, ox + 8, oy + 3);
    session_->place_sprite(SPRITE_MONSTER, ox + 12, oy + 8);
    session_->place_sprite(SPRITE_HELICOPTER, ox + 4, oy + 10);
    map_.set_tile_size(16);
    center_on_fraction((ox + 9) / static_cast<double>(CitySession::kWorldW),
                       (oy + 6) / static_cast<double>(CitySession::kWorldH));
    refresh();
}

void AppWindow::save_widget_png(Gtk::Widget &widget, const char *path)
{
    if (path == nullptr || path[0] == '\0') {
        return;
    }
    auto window = widget.get_window();
    if (!window) {
        return;
    }
    const int w = widget.get_allocated_width();
    const int h = widget.get_allocated_height();
    if (w < 2 || h < 2) {
        return;
    }
    try {
        window->process_updates(true);
        auto pix = Gdk::Pixbuf::create(window, 0, 0, w, h);
        pix->save(path, "png");
    } catch (const Glib::Error &) {
        return;
    }
}

void AppWindow::probe_zoom_if_requested()
{
    const char *path = std::getenv("LUNDUKE_CITY_ZOOM_PROBE");
    if (path == nullptr || path[0] == '\0') {
        return;
    }
    auto check_accel = [this](unsigned keyval) {
        map_.set_tile_size(10);
        gtk_accel_groups_activate(G_OBJECT(gobj()), keyval, GDK_CONTROL_MASK);
        return map_.tile_size();
    };
    auto check_key = [this](unsigned keyval, unsigned state) {
        map_.set_tile_size(10);
        GdkEventKey event{};
        event.type = GDK_KEY_PRESS;
        event.window = get_window() ? get_window()->gobj() : nullptr;
        event.keyval = keyval;
        event.state = state;
        event.send_event = 1;
        on_key_press_event(&event);
        return map_.tile_size();
    };
    std::ofstream out(path);
    out << "accel_equal " << check_accel(GDK_KEY_equal) << "\n";
    out << "accel_plus " << check_accel(GDK_KEY_plus) << "\n";
    out << "accel_kp_add " << check_accel(GDK_KEY_KP_Add) << "\n";
    out << "accel_minus " << check_accel(GDK_KEY_minus) << "\n";
    out << "key_equal " << check_key(GDK_KEY_equal, GDK_CONTROL_MASK) << "\n";
    out << "key_plus " << check_key(GDK_KEY_plus, GDK_CONTROL_MASK) << "\n";
    out << "key_plus_shift " << check_key(GDK_KEY_plus, GDK_CONTROL_MASK | GDK_SHIFT_MASK) << "\n";
    out << "key_kp_add " << check_key(GDK_KEY_KP_Add, GDK_CONTROL_MASK) << "\n";
    out << "key_minus " << check_key(GDK_KEY_minus, GDK_CONTROL_MASK) << "\n";
    out << "key_kp_sub " << check_key(GDK_KEY_KP_Subtract, GDK_CONTROL_MASK) << "\n";
    out << "key_equal_no_ctrl " << check_key(GDK_KEY_equal, 0) << "\n";
    map_.set_tile_size(16);
}

void AppWindow::grab_followup_shots()
{
    const char *scenario = std::getenv("LUNDUKE_CITY_SHOT_SCENARIO");
    if (scenario != nullptr && scenario[0] != '\0') {
        on_play_scenario();
    }
    const char *new_city = std::getenv("LUNDUKE_CITY_SHOT_NEWCITY");
    if (new_city != nullptr && new_city[0] != '\0') {
        on_new_city();
    }
    const char *rename = std::getenv("LUNDUKE_CITY_SHOT_RENAME");
    if (rename != nullptr && rename[0] != '\0') {
        on_rename_city();
    }
    const char *about = std::getenv("LUNDUKE_CITY_SHOT_ABOUT");
    if (about != nullptr && about[0] != '\0') {
        on_about();
    }
    const char *budget = std::getenv("LUNDUKE_CITY_SHOT_BUDGET");
    const char *overlay = std::getenv("LUNDUKE_CITY_SHOT_OVERLAY");
    const char *graphs = std::getenv("LUNDUKE_CITY_SHOT_GRAPHS");
    const char *evaluation = std::getenv("LUNDUKE_CITY_SHOT_EVAL");
    if (budget != nullptr && budget[0] != '\0') {
        on_budget();
    }
    if (overlay != nullptr && overlay[0] != '\0') {
        on_overlay(CitySession::MapLayer::Power);
    }
    if (graphs != nullptr && graphs[0] != '\0') {
        on_graphs();
    }
    if (evaluation != nullptr && evaluation[0] != '\0') {
        on_evaluation();
    }
    Glib::signal_timeout().connect_once(
        [this, budget, overlay, graphs, evaluation] {
            if (budget != nullptr && budget[0] != '\0') {
                save_widget_png(budget_window_, budget);
                budget_window_.hide();
            }
            if (overlay != nullptr && overlay[0] != '\0' && overlays_[0]) {
                save_widget_png(*overlays_[0], overlay);
                overlays_[0]->hide();
            }
            if (graphs != nullptr && graphs[0] != '\0') {
                save_widget_png(graphs_window_, graphs);
                graphs_window_.hide();
            }
            if (evaluation != nullptr && evaluation[0] != '\0') {
                save_widget_png(evaluation_window_, evaluation);
                evaluation_window_.hide();
            }
            if (const char *exit_flag = std::getenv("LUNDUKE_CITY_EXIT");
                exit_flag != nullptr && exit_flag[0] == '1') {
                hide();
            }
        },
        350);
}

void AppWindow::grab_screenshot_if_requested()
{
    probe_zoom_if_requested();
    const char *path = std::getenv("LUNDUKE_CITY_SCREENSHOT");
    if (path == nullptr || path[0] == '\0') {
        if (std::getenv("LUNDUKE_CITY_ZOOM_PROBE") != nullptr) {
            hide();
        }
        return;
    }
    prepare_demo_if_requested();
    Glib::signal_timeout().connect_once(
        [this, path] {
            int hover_x = CitySession::kWorldW / 2;
            int hover_y = CitySession::kWorldH / 2;
            const auto sprites = session_->sprites();
            for (const auto &dot : sprites) {
                if (dot.type == SPRITE_AIRPLANE || dot.type == SPRITE_MONSTER ||
                    dot.type == SPRITE_HELICOPTER || dot.type == SPRITE_SHIP ||
                    dot.type == SPRITE_TORNADO || dot.type == SPRITE_TRAIN) {
                    hover_x = dot.tile_x;
                    hover_y = dot.tile_y;
                    center_on_fraction((dot.tile_x + 2) / static_cast<double>(CitySession::kWorldW),
                                       (dot.tile_y + 2) / static_cast<double>(CitySession::kWorldH));
                    break;
                }
            }
            const char *preview = std::getenv("LUNDUKE_CITY_PREVIEW");
            if (preview != nullptr && preview[0] != '\0') {
                int index = kDefaultToolIndex;
                for (int i = 0; i < kToolCount; ++i) {
                    if (kTools[i].engine_id == TOOL_RESIDENTIAL) {
                        index = i;
                    }
                }
                tools_.set_selected(index);
                map_.set_hover_tile(hover_x, hover_y);
            }
            refresh();
            Glib::signal_timeout().connect_once(
                [this, path] {
                    if (auto window = get_window()) {
                        window->process_updates(true);
                    }
                    save_widget_png(*this, path);
                    grab_followup_shots();
                },
                200);
        },
        400);
}

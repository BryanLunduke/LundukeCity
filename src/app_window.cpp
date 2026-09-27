// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "app_window.hpp"

#include "city_session.hpp"
#include "tools.hpp"

#include <gdkmm/pixbuf.h>
#include <gdkmm/screen.h>
#include <glibmm/main.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/spinbutton.h>

#include <algorithm>
#include <cstdlib>

namespace {

const char kCss[] = R"CSS(
window.background {
  background-color: #c0c0c0;
}
menubar {
  background-color: #0000aa;
  background-image: none;
  border: none;
  box-shadow: none;
  padding: 0;
}
menubar > menuitem {
  color: #ffffff;
  background-color: #0000aa;
  background-image: none;
  border-radius: 0;
  padding: 4px 18px;
  margin: 0;
}
menubar > menuitem:hover,
menubar > menuitem:active {
  background-color: #ffffff;
  color: #0000aa;
}
menubar > menuitem label {
  color: inherit;
}
#status-bar,
#message-bar {
  background-color: #c0c0c0;
  border-style: solid;
  border-width: 2px;
  border-color: #404040 #f2f2f2 #f2f2f2 #404040;
}
#status-bar label,
#message-bar label {
  color: #000000;
  font-weight: bold;
  font-size: 13px;
}
#side-panel {
  background-color: #c0c0c0;
  border-right: 2px solid #808080;
}
#map-frame {
  border: 2px solid #404040;
}
)CSS";

} // namespace

AppWindow::~AppWindow() = default;

AppWindow::AppWindow()
{
    set_title("Lunduke City");
    set_default_size(1100, 740);
    session_ = std::make_unique<CitySession>();
    apply_css();
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
                                        700);
}

void AppWindow::apply_css()
{
    auto css = Gtk::CssProvider::create();
    try {
        css->load_from_data(kCss);
    } catch (const Glib::Error &) {
        return;
    }
    auto screen = Gdk::Screen::get_default();
    if (screen) {
        Gtk::StyleContext::add_provider_for_screen(screen, css,
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
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
    system_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    add_item(system_menu, "_Quit", GDK_KEY_q, [this] { hide(); });
    menu_bar_.append(*system);

    auto *options_menu = Gtk::manage(new Gtk::Menu());
    auto *options = Gtk::manage(new Gtk::MenuItem("_Options", true));
    options->set_submenu(*options_menu);

    auto_budget_item_ = Gtk::manage(new Gtk::CheckMenuItem("Auto _budget", true));
    auto_bulldoze_item_ = Gtk::manage(new Gtk::CheckMenuItem("Auto _bulldoze", true));
    disasters_item_ = Gtk::manage(new Gtk::CheckMenuItem("Enable _disasters", true));
    options_menu->append(*auto_budget_item_);
    options_menu->append(*auto_bulldoze_item_);
    options_menu->append(*disasters_item_);
    options_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));

    Gtk::RadioMenuItem::Group speed_group;
    const char *speed_labels[] = {"Pause", "Slow", "Medium", "Fast"};
    for (int i = 0; i < 4; ++i) {
        speed_items_[i] = Gtk::manage(new Gtk::RadioMenuItem(speed_group, speed_labels[i]));
        options_menu->append(*speed_items_[i]);
    }
    options_menu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
    add_item(options_menu, "Zoom _in", GDK_KEY_plus, [this] { zoom_by(2); });
    add_item(options_menu, "Zoom _out", GDK_KEY_minus, [this] { zoom_by(-2); });
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
    add_item(windows_menu, "_Evaluation", 0, sigc::mem_fun(*this, &AppWindow::on_evaluation));
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
    for (int i = 0; i < 4; ++i) {
        speed_items_[i]->signal_toggled().connect([this, i] {
            if (!updating_checks_ && speed_items_[i]->get_active()) {
                set_speed(i);
            }
        });
    }

    scroll_.get_hadjustment()->signal_value_changed().connect([this] { minimap_.queue_draw(); });
    scroll_.get_vadjustment()->signal_value_changed().connect([this] { minimap_.queue_draw(); });
}

void AppWindow::refresh()
{
    funds_label_.set_text(session_->funds_text());
    name_label_.set_text(session_->city_name());
    date_label_.set_text(session_->date_text());

    const auto now = std::chrono::steady_clock::now();
    const std::string engine_message = session_->message();
    if (!engine_message.empty() && engine_message != shown_engine_message_) {
        shown_engine_message_ = engine_message;
        message_label_.set_text(engine_message);
        hint_after_ = now + std::chrono::seconds(4);
    } else if (hint_after_.time_since_epoch().count() == 0 || now >= hint_after_) {
        message_label_.set_text(tool_hint_);
    }

    map_.queue_draw();
    minimap_.queue_draw();
    demand_.queue_draw();
}

void AppWindow::sync_option_checks()
{
    updating_checks_ = true;
    auto_budget_item_->set_active(session_->auto_budget());
    auto_bulldoze_item_->set_active(session_->auto_bulldoze());
    disasters_item_->set_active(session_->disasters());
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
    map_.set_tile_size(map_.tile_size() + delta);
    center_on_fraction(0.5, 0.5);
}

bool AppWindow::on_tick()
{
    session_->tick();
    refresh();
    return true;
}

void AppWindow::on_new_city()
{
    Gtk::Dialog dialog("New City", *this, true);
    dialog.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
    dialog.add_button("_Generate", Gtk::RESPONSE_OK);
    dialog.set_default_response(Gtk::RESPONSE_OK);
    auto *content = dialog.get_content_area();
    auto *label = Gtk::manage(new Gtk::Label("Name the new city:"));
    label->set_halign(Gtk::ALIGN_START);
    auto *entry = Gtk::manage(new Gtk::Entry());
    entry->set_text("New City");
    entry->set_activates_default(true);
    content->pack_start(*label, Gtk::PACK_SHRINK);
    content->pack_start(*entry, Gtk::PACK_SHRINK);
    content->set_border_width(8);
    content->set_spacing(6);
    dialog.show_all_children();
    if (dialog.run() != Gtk::RESPONSE_OK) {
        return;
    }
    session_->new_city(entry->get_text());
    session_->set_speed(speed_);
    sync_option_checks();
    show_tool_hint();
    refresh();
    center_on_fraction(0.5, 0.5);
}

void AppWindow::on_load_city()
{
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

void AppWindow::on_budget()
{
    Gtk::Dialog dialog("Budget", *this, true);
    dialog.add_button("_Close", Gtk::RESPONSE_CLOSE);
    auto *content = dialog.get_content_area();
    auto *summary = Gtk::manage(new Gtk::Label(session_->budget_text()));
    summary->set_halign(Gtk::ALIGN_START);
    auto *row = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 8));
    auto *tax_label = Gtk::manage(new Gtk::Label("Tax rate"));
    auto *tax = Gtk::manage(new Gtk::SpinButton());
    tax->set_range(0, 20);
    tax->set_increments(1, 5);
    tax->set_value(session_->tax());
    tax->signal_value_changed().connect([this, tax] {
        session_->set_tax(tax->get_value_as_int());
        refresh();
    });
    row->pack_start(*tax_label, Gtk::PACK_SHRINK);
    row->pack_start(*tax, Gtk::PACK_SHRINK);
    content->pack_start(*summary, Gtk::PACK_SHRINK);
    content->pack_start(*row, Gtk::PACK_SHRINK);
    content->set_border_width(8);
    content->set_spacing(8);
    dialog.show_all_children();
    dialog.run();
}

void AppWindow::on_evaluation()
{
    Gtk::MessageDialog dialog(*this, "Evaluation", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK, true);
    dialog.set_secondary_text(session_->evaluation_text());
    dialog.run();
}

void AppWindow::on_about()
{
    Gtk::MessageDialog dialog(*this, "Lunduke City 0.1", false, Gtk::MESSAGE_INFO, Gtk::BUTTONS_OK,
                              true);
    dialog.set_secondary_text(
        "A city-building game. The simulation is the Micropolis engine, "
        "used under GPL-3.0-or-later with the additional terms in NOTICE.\n\n"
        "Lunduke City is an independent project. It is not affiliated with "
        "or endorsed by Electronic Arts.");
    dialog.run();
}

void AppWindow::grab_screenshot_if_requested()
{
    const char *path = std::getenv("LUNDUKE_CITY_SCREENSHOT");
    if (path == nullptr || path[0] == '\0') {
        return;
    }
    auto window = get_window();
    if (!window) {
        return;
    }
    const int w = get_allocated_width();
    const int h = get_allocated_height();
    if (w < 2 || h < 2) {
        return;
    }
    try {
        auto pix = Gdk::Pixbuf::create(window, 0, 0, w, h);
        pix->save(path, "png");
    } catch (const Glib::Error &) {
        return;
    }
    if (const char *exit_flag = std::getenv("LUNDUKE_CITY_EXIT");
        exit_flag != nullptr && exit_flag[0] == '1') {
        hide();
    }
}

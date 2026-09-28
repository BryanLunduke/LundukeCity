// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "new_city_dialog.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/stack.h>

namespace {

void save_png(Gtk::Widget &widget, const std::string &path)
{
    if (path.empty()) {
        return;
    }
    auto window = widget.get_window();
    if (!window) {
        return;
    }
    const int width = widget.get_allocated_width();
    const int height = widget.get_allocated_height();
    if (width < 2 || height < 2) {
        return;
    }
    try {
        window->process_updates(true);
        auto pix = Gdk::Pixbuf::create(window, 0, 0, width, height);
        pix->save(path, "png");
    } catch (const Glib::Error &) {
        return;
    }
}

Gtk::Label *note(const char *text)
{
    auto *label = Gtk::manage(new Gtk::Label(text));
    label->set_halign(Gtk::ALIGN_START);
    label->set_xalign(0);
    label->set_line_wrap(true);
    label->set_max_width_chars(48);
    return label;
}

Gtk::Frame *group(const char *title, Gtk::Box &box)
{
    auto *frame = Gtk::manage(new Gtk::Frame(title));
    frame->set_border_width(2);
    box.set_border_width(6);
    box.set_spacing(2);
    frame->add(box);
    return frame;
}

const char *difficulty_blurb(int level)
{
    if (level == CitySession::kLevelMedium) {
        return "Medium ($10,000)";
    }
    if (level == CitySession::kLevelHard) {
        return "Hard ($5,000)";
    }
    return "Easy ($20,000)";
}

} // namespace

bool run_new_city_wizard(Gtk::Window &parent, CitySession::NewCitySpec &spec, const std::string &shot_path)
{
    Gtk::Dialog dialog("New City", parent, true);
    dialog.set_default_size(520, 520);
    dialog.set_border_width(4);

    auto *cancel = Gtk::manage(new Gtk::Button("_Cancel", true));
    auto *back = Gtk::manage(new Gtk::Button("_Back", true));
    auto *next = Gtk::manage(new Gtk::Button("_Next", true));
    auto *generate = Gtk::manage(new Gtk::Button("_Generate", true));
    dialog.get_action_area()->pack_start(*cancel, Gtk::PACK_SHRINK);
    dialog.get_action_area()->pack_end(*generate, Gtk::PACK_SHRINK);
    dialog.get_action_area()->pack_end(*next, Gtk::PACK_SHRINK);
    dialog.get_action_area()->pack_end(*back, Gtk::PACK_SHRINK);

    auto *stack = Gtk::manage(new Gtk::Stack());
    stack->set_transition_type(Gtk::STACK_TRANSITION_TYPE_NONE);
    stack->set_vexpand(true);

    auto *name_page = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
    name_page->set_border_width(12);
    name_page->pack_start(*note("Name the new city."), Gtk::PACK_SHRINK);
    auto *name_entry = Gtk::manage(new Gtk::Entry());
    name_entry->set_text("New City");
    name_page->pack_start(*name_entry, Gtk::PACK_SHRINK);
    name_page->pack_start(*note("The next step sets the difficulty, the terrain the generator "
                                "knows how to build, and the map seed."),
                          Gtk::PACK_SHRINK);

    auto *land = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
    land->set_border_width(8);

    Gtk::RadioButtonGroup level_group;
    auto *easy = Gtk::manage(new Gtk::RadioButton(level_group, "Easy ($20,000)"));
    auto *medium = Gtk::manage(new Gtk::RadioButton(level_group, "Medium ($10,000)"));
    auto *hard = Gtk::manage(new Gtk::RadioButton(level_group, "Hard ($5,000)"));
    easy->set_active(true);
    Gtk::Box level_box(Gtk::ORIENTATION_VERTICAL, 2);
    level_box.pack_start(*easy, Gtk::PACK_SHRINK);
    level_box.pack_start(*medium, Gtk::PACK_SHRINK);
    level_box.pack_start(*hard, Gtk::PACK_SHRINK);
    land->pack_start(*group("Difficulty", level_box), Gtk::PACK_SHRINK);
    land->pack_start(*note("Starting cash and the engine tax and road-cost tables."), Gtk::PACK_SHRINK);

    Gtk::RadioButtonGroup island_group;
    auto *island_default = Gtk::manage(new Gtk::RadioButton(island_group, "Generator default (about 1 in 10)"));
    auto *island_never = Gtk::manage(new Gtk::RadioButton(island_group, "Mainland"));
    auto *island_always = Gtk::manage(new Gtk::RadioButton(island_group, "Island"));
    island_default->set_active(true);
    Gtk::Box island_box(Gtk::ORIENTATION_VERTICAL, 2);
    island_box.pack_start(*island_default, Gtk::PACK_SHRINK);
    island_box.pack_start(*island_never, Gtk::PACK_SHRINK);
    island_box.pack_start(*island_always, Gtk::PACK_SHRINK);
    land->pack_start(*group("Island", island_box), Gtk::PACK_SHRINK);

    Gtk::RadioButtonGroup river_group;
    auto *river_default = Gtk::manage(new Gtk::RadioButton(river_group, "Generator default"));
    auto *river_none = Gtk::manage(new Gtk::RadioButton(river_group, "No river"));
    auto *river_curvy = Gtk::manage(new Gtk::RadioButton(river_group, "Curvier river"));
    river_default->set_active(true);
    Gtk::Box river_box(Gtk::ORIENTATION_VERTICAL, 2);
    river_box.pack_start(*river_default, Gtk::PACK_SHRINK);
    river_box.pack_start(*river_none, Gtk::PACK_SHRINK);
    river_box.pack_start(*river_curvy, Gtk::PACK_SHRINK);
    land->pack_start(*group("Rivers", river_box), Gtk::PACK_SHRINK);

    Gtk::RadioButtonGroup lake_group;
    auto *lake_default = Gtk::manage(new Gtk::RadioButton(lake_group, "Generator default"));
    auto *lake_none = Gtk::manage(new Gtk::RadioButton(lake_group, "No lakes"));
    auto *lake_many = Gtk::manage(new Gtk::RadioButton(lake_group, "Many lakes"));
    lake_default->set_active(true);
    Gtk::Box lake_box(Gtk::ORIENTATION_VERTICAL, 2);
    lake_box.pack_start(*lake_default, Gtk::PACK_SHRINK);
    lake_box.pack_start(*lake_none, Gtk::PACK_SHRINK);
    lake_box.pack_start(*lake_many, Gtk::PACK_SHRINK);
    land->pack_start(*group("Lakes", lake_box), Gtk::PACK_SHRINK);

    Gtk::RadioButtonGroup tree_group;
    auto *tree_default = Gtk::manage(new Gtk::RadioButton(tree_group, "Generator default"));
    auto *tree_none = Gtk::manage(new Gtk::RadioButton(tree_group, "No trees"));
    auto *tree_wooded = Gtk::manage(new Gtk::RadioButton(tree_group, "Wooded"));
    tree_default->set_active(true);
    Gtk::Box tree_box(Gtk::ORIENTATION_VERTICAL, 2);
    tree_box.pack_start(*tree_default, Gtk::PACK_SHRINK);
    tree_box.pack_start(*tree_none, Gtk::PACK_SHRINK);
    tree_box.pack_start(*tree_wooded, Gtk::PACK_SHRINK);
    land->pack_start(*group("Trees", tree_box), Gtk::PACK_SHRINK);

    auto seed_adjust = Gtk::Adjustment::create(0, 0, 999999999, 1, 100, 0);
    auto *seed = Gtk::manage(new Gtk::SpinButton(seed_adjust, 1, 0));
    seed->set_numeric(true);
    seed->set_snap_to_ticks(true);
    Gtk::Box seed_box(Gtk::ORIENTATION_VERTICAL, 4);
    seed_box.pack_start(*seed, Gtk::PACK_SHRINK);
    seed_box.pack_start(*note("0 takes a seed from the clock. Any other integer is passed "
                              "straight to the map generator."),
                        Gtk::PACK_SHRINK);
    land->pack_start(*group("Seed", seed_box), Gtk::PACK_SHRINK);

    auto *scroller = Gtk::manage(new Gtk::ScrolledWindow());
    scroller->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    scroller->set_shadow_type(Gtk::SHADOW_IN);
    scroller->add(*land);

    auto *summary_page = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 8));
    summary_page->set_border_width(12);
    auto *summary = Gtk::manage(new Gtk::Label());
    summary->set_halign(Gtk::ALIGN_START);
    summary->set_xalign(0);
    summary->set_line_wrap(true);
    summary->set_max_width_chars(48);
    summary_page->pack_start(*note("Generate this map?"), Gtk::PACK_SHRINK);
    summary_page->pack_start(*summary, Gtk::PACK_SHRINK);

    stack->add(*name_page, "name");
    stack->add(*scroller, "land");
    stack->add(*summary_page, "generate");

    auto *content = dialog.get_content_area();
    content->pack_start(*stack, Gtk::PACK_EXPAND_WIDGET);

    int page = 0;
    auto chosen_level = [&] {
        if (medium->get_active()) {
            return CitySession::kLevelMedium;
        }
        if (hard->get_active()) {
            return CitySession::kLevelHard;
        }
        return CitySession::kLevelEasy;
    };
    auto chosen_island = [&] {
        if (island_never->get_active()) {
            return CitySession::kTerrainOff;
        }
        if (island_always->get_active()) {
            return CitySession::kIslandAlways;
        }
        return CitySession::kTerrainDefault;
    };
    auto chosen_rivers = [&] {
        if (river_none->get_active()) {
            return CitySession::kTerrainOff;
        }
        if (river_curvy->get_active()) {
            return CitySession::kRiversCurvy;
        }
        return CitySession::kTerrainDefault;
    };
    auto chosen_lakes = [&] {
        if (lake_none->get_active()) {
            return CitySession::kTerrainOff;
        }
        if (lake_many->get_active()) {
            return CitySession::kLakesMany;
        }
        return CitySession::kTerrainDefault;
    };
    auto chosen_trees = [&] {
        if (tree_none->get_active()) {
            return CitySession::kTerrainOff;
        }
        if (tree_wooded->get_active()) {
            return CitySession::kTreesWooded;
        }
        return CitySession::kTerrainDefault;
    };
    auto island_blurb = [&] {
        const int value = chosen_island();
        if (value == CitySession::kTerrainOff) {
            return "Mainland";
        }
        if (value == CitySession::kIslandAlways) {
            return "Island";
        }
        return "Generator default (about 1 in 10)";
    };
    auto rivers_blurb = [&] {
        const int value = chosen_rivers();
        if (value == CitySession::kTerrainOff) {
            return "No river";
        }
        if (value == CitySession::kRiversCurvy) {
            return "Curvier river";
        }
        return "Generator default";
    };
    auto lakes_blurb = [&] {
        const int value = chosen_lakes();
        if (value == CitySession::kTerrainOff) {
            return "No lakes";
        }
        if (value == CitySession::kLakesMany) {
            return "Many lakes";
        }
        return "Generator default";
    };
    auto trees_blurb = [&] {
        const int value = chosen_trees();
        if (value == CitySession::kTerrainOff) {
            return "No trees";
        }
        if (value == CitySession::kTreesWooded) {
            return "Wooded";
        }
        return "Generator default";
    };

    auto fill_spec = [&] {
        spec = CitySession::NewCitySpec{};
        spec.name = name_entry->get_text();
        spec.difficulty = chosen_level();
        spec.island = chosen_island();
        spec.rivers = chosen_rivers();
        spec.lakes = chosen_lakes();
        spec.trees = chosen_trees();
        spec.seed = seed->get_value_as_int();
    };
    auto refresh_summary = [&] {
        fill_spec();
        std::string name = spec.name;
        if (name.empty()) {
            name = "New City";
        }
        std::string seed_text = spec.seed == 0 ? "chosen from the clock" : std::to_string(spec.seed);
        summary->set_text(std::string("Name: ") + name + "\nDifficulty: " + difficulty_blurb(spec.difficulty) +
                          "\nIsland: " + island_blurb() + "\nRivers: " + rivers_blurb() + "\nLakes: " +
                          lakes_blurb() + "\nTrees: " + trees_blurb() + "\nSeed: " + seed_text);
    };
    auto show_page = [&](int next_page) {
        page = next_page;
        if (page < 0) {
            page = 0;
        }
        if (page > 2) {
            page = 2;
        }
        if (page == 0) {
            stack->set_visible_child(*name_page);
        } else if (page == 1) {
            stack->set_visible_child(*scroller);
        } else {
            refresh_summary();
            stack->set_visible_child(*summary_page);
        }
        back->set_sensitive(page > 0);
        next->set_visible(page < 2);
        generate->set_visible(page == 2);
        dialog.set_title(page == 0 ? "New City" : (page == 1 ? "New City — Land" : "New City — Generate"));
    };

    back->signal_clicked().connect([&] { show_page(page - 1); });
    next->signal_clicked().connect([&] { show_page(page + 1); });
    cancel->signal_clicked().connect([&] { dialog.response(Gtk::RESPONSE_CANCEL); });
    generate->signal_clicked().connect([&] {
        fill_spec();
        dialog.response(Gtk::RESPONSE_OK);
    });

    dialog.show_all_children();
    show_page(shot_path.empty() ? 0 : 1);
    if (!shot_path.empty()) {
        Glib::signal_timeout().connect_once(
            [&dialog, &shot_path] {
                save_png(dialog, shot_path);
                dialog.response(Gtk::RESPONSE_CANCEL);
            },
            400);
    }

    const int response = dialog.run();
    dialog.hide();
    if (response != Gtk::RESPONSE_OK) {
        return false;
    }
    fill_spec();
    return true;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "about_dialog.hpp"

#include "version.hpp"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/separator.h>

namespace {

Gtk::Label *paragraph(const char *text)
{
    auto *label = Gtk::manage(new Gtk::Label(text));
    label->set_halign(Gtk::ALIGN_START);
    label->set_xalign(0);
    label->set_line_wrap(true);
    label->set_max_width_chars(52);
    label->set_line_wrap_mode(Pango::WRAP_WORD_CHAR);
    return label;
}

Gtk::Label *heading(const char *text)
{
    auto *label = Gtk::manage(new Gtk::Label());
    label->set_markup(std::string("<b>") + text + "</b>");
    label->set_halign(Gtk::ALIGN_START);
    label->set_margin_top(8);
    return label;
}

} // namespace

AboutDialog::AboutDialog(Gtk::Window &parent)
    : Gtk::Dialog("About Lunduke City", parent, true)
{
    add_button("_Close", Gtk::RESPONSE_CLOSE);
    set_default_response(Gtk::RESPONSE_CLOSE);
    set_default_size(480, 420);

    auto *content = get_content_area();
    content->set_border_width(16);
    content->set_spacing(6);

    auto *title = Gtk::manage(new Gtk::Label());
    title->set_markup("<span size=\"xx-large\" weight=\"bold\">Lunduke City</span>");
    title->set_halign(Gtk::ALIGN_START);

    auto *version = Gtk::manage(new Gtk::Label(std::string("Version ") + kReleaseTrack));
    version->set_halign(Gtk::ALIGN_START);

    content->pack_start(*title, Gtk::PACK_SHRINK);
    content->pack_start(*version, Gtk::PACK_SHRINK);
    content->pack_start(*paragraph("A city-building game for LCOS. Zone the land, lay out services, "
                                   "balance the budget, and watch the city grow."),
                        Gtk::PACK_SHRINK);
    content->pack_start(*Gtk::manage(new Gtk::Separator()), Gtk::PACK_SHRINK);
    content->pack_start(*heading("License"), Gtk::PACK_SHRINK);
    content->pack_start(
        *paragraph("The simulation, the 16-pixel tiles, the sprites, and the sounds are from the "
                   "Micropolis engine. They are used under GPL-3.0-or-later. The additional terms "
                   "for that engine and those assets are in NOTICE. The GNU GPL version 3 is in COPYING."),
        Gtk::PACK_SHRINK);
    content->pack_start(*heading("Independent project"), Gtk::PACK_SHRINK);
    content->pack_start(
        *paragraph("Lunduke City is an independent project. It is not SimCity and it is not "
                   "affiliated with or endorsed by Electronic Arts. No trademark rights are granted; "
                   "see NOTICE."),
        Gtk::PACK_SHRINK);
    show_all_children();
}

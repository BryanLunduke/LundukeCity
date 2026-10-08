// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 7 window checks: open a saved city, report a bad file, keep the
// welcome line with the clock, propose a save name, and show one population.

#include "app_window.hpp"
#include "city_session.hpp"
#include "graph_legend.hpp"
#include "graph_palette.hpp"
#include "graphs_window.hpp"
#include "save_path.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/main.h>
#include <gtkmm/container.h>
#include <gtkmm/dialog.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/label.h>
#include <gtkmm/main.h>
#include <gtkmm/messagedialog.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

const char *shot_dir = nullptr;

void pump()
{
    for (int i = 0; i < 12; ++i) {
        while (Gtk::Main::events_pending()) {
            Gtk::Main::iteration(false);
        }
    }
}

void save_png(Gtk::Widget &widget, const char *name)
{
    if (shot_dir == nullptr || name == nullptr) {
        return;
    }
    auto window = widget.get_window();
    if (!window) {
        std::cerr << "no window for " << name << "\n";
        return;
    }
    window->process_updates(true);
    const int w = widget.get_allocated_width();
    const int h = widget.get_allocated_height();
    // Dialogs are short. Still keep the whole window, and skip a widget
    // that has not been given a real size.
    if (w < 200 || h < 80) {
        std::cerr << name << " is " << w << "x" << h << "\n";
        return;
    }
    try {
        auto pix = Gdk::Pixbuf::create(window, 0, 0, w, h);
        pix->save(std::string(shot_dir) + "/" + name, "png");
    } catch (const Glib::Error &error) {
        std::cerr << name << " " << error.what() << "\n";
    }
}

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

bool rect_of(Gtk::Widget &widget, Gtk::Widget &origin, Rect &out)
{
    int x = 0;
    int y = 0;
    if (!widget.translate_coordinates(origin, 0, 0, x, y)) {
        return false;
    }
    out.x = x;
    out.y = y;
    out.w = widget.get_allocated_width();
    out.h = widget.get_allocated_height();
    return out.w > 0 && out.h > 0;
}

void walk(Gtk::Widget *widget, std::vector<Gtk::Widget *> &out)
{
    if (widget == nullptr) {
        return;
    }
    out.push_back(widget);
    if (auto *container = dynamic_cast<Gtk::Container *>(widget)) {
        for (Gtk::Widget *child : container->get_children()) {
            walk(child, out);
        }
    }
}

std::string canon(const std::string &path)
{
    std::error_code ec;
    const auto cleaned = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return cleaned.string();
}

int channel_delta(int r, int g, int b, int er, int eg, int eb)
{
    return std::abs(r - er) + std::abs(g - eg) + std::abs(b - eb);
}

} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        return fail(1, "no DISPLAY; this GUI test must fail rather than skip or start its own server");
    }
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind("--shots=", 0) == 0) {
            shot_dir = argv[i] + 8;
        }
    }
    if (shot_dir != nullptr) {
        std::error_code ec;
        std::filesystem::create_directories(shot_dir, ec);
    }

    char home_template[] = "/tmp/lunduke-round7-ui-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return fail(1, "could not make a home directory");
    }
    setenv("HOME", home, 1);
    const std::string xdg_sentinel = "/tmp/xfce4-must-not-be-written-round7";
    setenv("XDG_CONFIG_HOME", xdg_sentinel.c_str(), 1);
    const std::string remembered = std::string(home) + "/saved-maps";
    std::error_code ec;
    std::filesystem::create_directories(remembered, ec);
    remember_city_folder(remembered + "/placeholder.cty");

    char file_template[] = "/tmp/lunduke-round7-files-XXXXXX";
    char *files = mkdtemp(file_template);
    if (files == nullptr) {
        return fail(1, "could not make a directory for city files");
    }
    const std::string good = std::string(files) + "/harbor.cty";
    const std::string corrupt = std::string(files) + "/corrupt.cty";
    const std::string missing = std::string(files) + "/missing.cty";
    {
        CitySession writer;
        writer.new_city("Harbor Town", 11);
        if (!writer.save_city_as(good)) {
            return fail(2, "could not write the city fixture");
        }
    }
    {
        std::ofstream out(corrupt, std::ios::binary);
        out << "not a city\n";
    }

    Gtk::Main kit(argc, argv);
    AppWindow window;
    window.set_default_size(1100, 740);
    window.resize(1100, 740);
    window.show_all();
    window.present();
    pump();

    const int startup = hostile_review_window_probe(window, 6);
    if ((startup & 16) == 0 || (startup & 4) == 0 || (startup & 1) == 0) {
        std::cerr << "startup bits " << startup << "\n";
        return fail(3, "the first screen is not paused with the welcome line");
    }
    save_png(window, "welcome_paused.png");

    std::string offered_name;
    std::string offered_folder;
    bool saw_save = false;
    sigc::connection save_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::FileChooserDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Save City") {
                continue;
            }
            offered_name = dialog->get_current_name();
            offered_folder = dialog->get_current_folder();
            saw_save = true;
            save_png(*dialog, "save_city_name.png");
            dialog->response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        return true;
    });
    if (hostile_review_window_probe(window, 15) != 1 || !saw_save || offered_name != "New City.cty" ||
        canon(offered_folder) != canon(remembered)) {
        save_idle.disconnect();
        std::cerr << "name '" << offered_name << "' folder '" << offered_folder << "'\n";
        return fail(4, "Save City did not propose New City.cty in the remembered folder");
    }
    save_idle.disconnect();

    if (hostile_review_window_probe(window, 13) != 1) {
        return fail(5, "the welcome line still says the clock is paused while it is running");
    }
    save_png(window, "welcome_running.png");
    if (hostile_review_window_probe(window, 14) != 1) {
        return fail(6, "pausing the clock did not put the paused sentence back");
    }

    if (hostile_review_window_probe(window, 5) != 1) {
        return fail(7, "Dullsville did not start");
    }
    offered_name.clear();
    offered_folder.clear();
    saw_save = false;
    sigc::connection dull_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::FileChooserDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Save City") {
                continue;
            }
            offered_name = dialog->get_current_name();
            offered_folder = dialog->get_current_folder();
            saw_save = true;
            dialog->response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        return true;
    });
    if (hostile_review_window_probe(window, 15) != 1 || !saw_save || offered_name != "Dullsville.cty" ||
        canon(offered_folder) != canon(remembered)) {
        dull_idle.disconnect();
        std::cerr << "dull name '" << offered_name << "' folder '" << offered_folder << "'\n";
        return fail(8, "Save City did not propose the scenario's name");
    }
    dull_idle.disconnect();

    if (hostile_review_window_probe(window, 18) != 1) {
        return fail(9, "could not save the city to a named file");
    }
    offered_name.clear();
    offered_folder.clear();
    saw_save = false;
    sigc::connection named_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::FileChooserDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Save City") {
                continue;
            }
            offered_name = dialog->get_current_name();
            offered_folder = dialog->get_current_folder();
            saw_save = true;
            dialog->response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        return true;
    });
    if (hostile_review_window_probe(window, 15) != 1 || !saw_save || offered_name != "harbor.cty" ||
        canon(offered_folder) != canon("/tmp/lunduke-round7-named")) {
        named_idle.disconnect();
        std::cerr << "file name '" << offered_name << "' folder '" << offered_folder << "'\n";
        return fail(10, "Save City did not propose the current file name and folder");
    }
    named_idle.disconnect();

    if (window.open_city_file(good) != AppWindow::CityFileOpen::Loaded ||
        window.get_title().find("Harbor Town") == std::string::npos) {
        std::cerr << "title '" << window.get_title() << "'\n";
        return fail(11, "opening a saved city file did not load that city");
    }

    bool saw_error = false;
    std::string error_path;
    sigc::connection error_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::MessageDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Could not load city") {
                continue;
            }
            saw_error = true;
            std::vector<Gtk::Widget *> widgets;
            walk(dialog, widgets);
            for (Gtk::Widget *widget : widgets) {
                if (auto *label = dynamic_cast<Gtk::Label *>(widget)) {
                    error_path += label->get_text();
                    error_path.push_back('\n');
                }
            }
            save_png(*dialog, "load_error.png");
            dialog->response(Gtk::RESPONSE_OK);
            return false;
        }
        return true;
    });
    if (window.open_city_file(corrupt) != AppWindow::CityFileOpen::Failed || !saw_error ||
        error_path.find(corrupt) == std::string::npos ||
        window.get_title().find("Harbor Town") == std::string::npos) {
        error_idle.disconnect();
        std::cerr << "error '" << error_path << "' title '" << window.get_title() << "'\n";
        return fail(12, "a corrupt city file did not report the path and keep the current city");
    }
    error_idle.disconnect();

    saw_error = false;
    error_path.clear();
    sigc::connection missing_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::MessageDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible() || dialog->get_title() != "Could not load city") {
                continue;
            }
            saw_error = true;
            std::vector<Gtk::Widget *> widgets;
            walk(dialog, widgets);
            for (Gtk::Widget *widget : widgets) {
                if (auto *label = dynamic_cast<Gtk::Label *>(widget)) {
                    error_path += label->get_text();
                    error_path.push_back('\n');
                }
            }
            dialog->response(Gtk::RESPONSE_OK);
            return false;
        }
        return true;
    });
    if (window.open_city_file(missing) != AppWindow::CityFileOpen::Failed || !saw_error ||
        error_path.find(missing) == std::string::npos) {
        missing_idle.disconnect();
        std::cerr << "missing '" << error_path << "'\n";
        return fail(13, "a missing city file did not show an error dialog");
    }
    missing_idle.disconnect();

    if (hostile_review_window_probe(window, 19) != 1) {
        return fail(14, "the city could not be marked unsaved");
    }
    bool cancelled = false;
    sigc::connection cancel_idle = Glib::signal_idle().connect([&]() -> bool {
        for (Gtk::Window *top : Gtk::Window::list_toplevels()) {
            auto *dialog = dynamic_cast<Gtk::MessageDialog *>(top);
            if (dialog == nullptr || !dialog->get_visible()) {
                continue;
            }
            std::string text;
            std::vector<Gtk::Widget *> widgets;
            walk(dialog, widgets);
            for (Gtk::Widget *widget : widgets) {
                if (auto *label = dynamic_cast<Gtk::Label *>(widget)) {
                    text += label->get_text();
                }
            }
            if (text.find("Save changes") == std::string::npos) {
                continue;
            }
            cancelled = true;
            dialog->response(Gtk::RESPONSE_CANCEL);
            return false;
        }
        return true;
    });
    if (window.open_city_file(good) != AppWindow::CityFileOpen::Cancelled || !cancelled ||
        window.get_title().find("Dirty Town") == std::string::npos) {
        cancel_idle.disconnect();
        std::cerr << "title '" << window.get_title() << "' cancelled " << cancelled << "\n";
        return fail(15, "opening a city did not ask before discarding unsaved work");
    }
    cancel_idle.disconnect();

    const std::string stored = std::string(home) + "/.config/lunduke-city/last-folder";
    std::ifstream folder_file(stored);
    std::string stored_line;
    std::getline(folder_file, stored_line);
    if (!folder_file || stored_line.find("/xfce") != std::string::npos ||
        stored.rfind(std::string(home) + "/.config/lunduke-city/", 0) != 0) {
        std::cerr << "stored '" << stored_line << "'\n";
        return fail(16, "the remembered folder was not kept under ~/.config/lunduke-city");
    }
    (void)xdg_sentinel;

    CitySession tokyo;
    if (!tokyo.load_scenario(CitySession::scenario_def(4).id) || tokyo.city_name() != "Tokyo") {
        return fail(17, "Tokyo did not load for the graphs window");
    }
    const long census = tokyo.evaluation().population;
    GraphsWindow graphs;
    graphs.set_session(&tokyo);
    graphs.set_default_size(900, 640);
    graphs.resize(900, 640);
    graphs.present_graphs();
    pump();
    std::vector<Gtk::Widget *> widgets;
    walk(graphs.get_child(), widgets);
    std::string header;
    std::string legend;
    for (Gtk::Widget *widget : widgets) {
        auto *label = dynamic_cast<Gtk::Label *>(widget);
        if (label == nullptr || !label->get_visible()) {
            continue;
        }
        if (label->get_name() == "graph-population") {
            header = label->get_text();
        }
        if (label->get_name() == "graph-legend-label" && label->get_text().rfind("Population", 0) == 0) {
            legend = label->get_text();
        }
    }
    const std::string expect = graph_legend_caption("Population", GraphLegendKind::Population, census);
    if (header.empty() || header != legend || header != expect || census < 1000) {
        std::cerr << "header '" << header << "' legend '" << legend << "' census " << census << "\n";
        return fail(18, "the Graphs header and the Population legend disagree");
    }
    save_png(graphs, "graphs_population.png");

    auto gdk = graphs.get_window();
    if (!gdk) {
        return fail(19, "the graphs window has no drawable");
    }
    gdk->process_updates(true);
    Glib::RefPtr<Gdk::Pixbuf> pix;
    try {
        pix = Gdk::Pixbuf::create(gdk, 0, 0, graphs.get_allocated_width(), graphs.get_allocated_height());
    } catch (const Glib::Error &error) {
        std::cerr << error.what() << "\n";
        return fail(19, "could not read the graphs window");
    }
    const bool dark = header.find("Population") != std::string::npos &&
                      pix->get_pixels()[0] + pix->get_pixels()[1] + pix->get_pixels()[2] < 200;
    for (int series = 0; series < kGraphSeriesCount; ++series) {
        Gtk::Label *label = nullptr;
        for (Gtk::Widget *widget : widgets) {
            auto *candidate = dynamic_cast<Gtk::Label *>(widget);
            if (candidate == nullptr || candidate->get_name() != "graph-legend-label") {
                continue;
            }
            if (candidate->get_text().rfind(kGraphSeries[series].name, 0) == 0) {
                label = candidate;
                break;
            }
        }
        if (label == nullptr || label->get_parent() == nullptr) {
            return fail(20, "a graph series has no legend label");
        }
        Gtk::DrawingArea *swatch = nullptr;
        if (auto *row = dynamic_cast<Gtk::Container *>(label->get_parent())) {
            for (Gtk::Widget *child : row->get_children()) {
                if (child->get_name() == "graph-legend-swatch") {
                    swatch = dynamic_cast<Gtk::DrawingArea *>(child);
                }
            }
        }
        Rect swatch_rect;
        if (swatch == nullptr || !rect_of(*swatch, graphs, swatch_rect)) {
            return fail(21, "a graph series swatch is not on screen");
        }
        const int er = dark ? kGraphSeries[series].dark_r : kGraphSeries[series].light_r;
        const int eg = dark ? kGraphSeries[series].dark_g : kGraphSeries[series].light_g;
        const int eb = dark ? kGraphSeries[series].dark_b : kGraphSeries[series].light_b;
        const int channels = pix->get_n_channels();
        const int stride = pix->get_rowstride();
        const guint8 *pixels = pix->get_pixels();
        int best = 1000000;
        const int y = swatch_rect.y + swatch_rect.h / 2;
        for (int x = swatch_rect.x; x < swatch_rect.x + swatch_rect.w; ++x) {
            if (x < 0 || y < 0 || x >= pix->get_width() || y >= pix->get_height()) {
                continue;
            }
            const guint8 *p = pixels + y * stride + x * channels;
            best = std::min(best, channel_delta(p[0], p[1], p[2], er, eg, eb));
        }
        if (best > 90) {
            std::cerr << kGraphSeries[series].name << " swatch delta " << best << "\n";
            return fail(22, "a graph swatch is not its series color");
        }
    }

    hostile_review_session_probe(tokyo, 2);
    graphs.sync();
    pump();
    widgets.clear();
    walk(graphs.get_child(), widgets);
    header.clear();
    legend.clear();
    for (Gtk::Widget *widget : widgets) {
        auto *label = dynamic_cast<Gtk::Label *>(widget);
        if (label == nullptr) {
            continue;
        }
        if (label->get_name() == "graph-population") {
            header = label->get_text();
        }
        if (label->get_name() == "graph-legend-label" && label->get_text().rfind("Population", 0) == 0) {
            legend = label->get_text();
        }
    }
    if (header != "Population: 1,280" || legend != header) {
        std::cerr << "after census header '" << header << "' legend '" << legend << "'\n";
        return fail(23, "the Population legend did not follow the census");
    }

    graphs.hide();
    window.hide();
    return 0;
}

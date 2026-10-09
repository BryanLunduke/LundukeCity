// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Round 7 checks that do not need a window: graph colors, the save name,
// the remembered folder, and the desktop / MIME packaging files.

#include "graph_palette.hpp"
#include "save_path.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cstdlib>
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

double linear_channel(double channel)
{
    const double c = channel / 255.0;
    if (c <= 0.04045) {
        return c / 12.92;
    }
    return std::pow((c + 0.055) / 1.055, 2.4);
}

struct Lab {
    double L;
    double a;
    double b;
};

Lab rgb_to_lab(int r, int g, int b)
{
    const double R = linear_channel(r);
    const double G = linear_channel(g);
    const double B = linear_channel(b);
    const double x = R * 0.4124564 + G * 0.3575761 + B * 0.1804375;
    const double y = R * 0.2126729 + G * 0.7151522 + B * 0.0721750;
    const double z = R * 0.0193339 + G * 0.1191920 + B * 0.9503041;
    const double xr = x / 0.95047;
    const double yr = y / 1.0;
    const double zr = z / 1.08883;
    auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : (7.787037 * t + 16.0 / 116.0); };
    const double fx = f(xr);
    const double fy = f(yr);
    const double fz = f(zr);
    return Lab{116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz)};
}

double ciede2000(const Lab &left, const Lab &right)
{
    const double L1 = left.L;
    const double a1 = left.a;
    const double b1 = left.b;
    const double L2 = right.L;
    const double a2 = right.a;
    const double b2 = right.b;
    const double C1 = std::hypot(a1, b1);
    const double C2 = std::hypot(a2, b2);
    const double Cbar = (C1 + C2) / 2.0;
    const double cbar7 = std::pow(Cbar, 7.0);
    const double G = 0.5 * (1.0 - std::sqrt(cbar7 / (cbar7 + std::pow(25.0, 7.0))));
    const double a1p = (1.0 + G) * a1;
    const double a2p = (1.0 + G) * a2;
    const double C1p = std::hypot(a1p, b1);
    const double C2p = std::hypot(a2p, b2);
    auto hue = [](double a, double b) {
        if (a == 0.0 && b == 0.0) {
            return 0.0;
        }
        double h = std::atan2(b, a) * 180.0 / M_PI;
        if (h < 0.0) {
            h += 360.0;
        }
        return h;
    };
    const double h1p = hue(a1p, b1);
    const double h2p = hue(a2p, b2);
    const double dLp = L2 - L1;
    const double dCp = C2p - C1p;
    double dhp = 0.0;
    if (C1p * C2p != 0.0) {
        dhp = h2p - h1p;
        if (dhp > 180.0) {
            dhp -= 360.0;
        } else if (dhp < -180.0) {
            dhp += 360.0;
        }
    }
    const double dHp = 2.0 * std::sqrt(C1p * C2p) * std::sin((dhp * M_PI / 180.0) / 2.0);
    const double Lbar = (L1 + L2) / 2.0;
    const double Cbarp = (C1p + C2p) / 2.0;
    double hbar = h1p + h2p;
    if (C1p * C2p != 0.0) {
        hbar = (h1p + h2p) / 2.0;
        if (std::fabs(h1p - h2p) > 180.0) {
            hbar += (h1p + h2p < 360.0) ? 180.0 : -180.0;
        }
    }
    const double T = 1.0 - 0.17 * std::cos((hbar - 30.0) * M_PI / 180.0) +
                     0.24 * std::cos((2.0 * hbar) * M_PI / 180.0) +
                     0.32 * std::cos((3.0 * hbar + 6.0) * M_PI / 180.0) -
                     0.20 * std::cos((4.0 * hbar - 63.0) * M_PI / 180.0);
    const double dtheta = 30.0 * std::exp(-std::pow((hbar - 275.0) / 25.0, 2.0));
    const double cbarp7 = std::pow(Cbarp, 7.0);
    const double Rc = 2.0 * std::sqrt(cbarp7 / (cbarp7 + std::pow(25.0, 7.0)));
    const double Sl = 1.0 + (0.015 * std::pow(Lbar - 50.0, 2.0)) / std::sqrt(20.0 + std::pow(Lbar - 50.0, 2.0));
    const double Sc = 1.0 + 0.045 * Cbarp;
    const double Sh = 1.0 + 0.015 * Cbarp * T;
    const double Rt = -std::sin(2.0 * dtheta * M_PI / 180.0) * Rc;
    return std::sqrt(std::pow(dLp / Sl, 2.0) + std::pow(dCp / Sc, 2.0) + std::pow(dHp / Sh, 2.0) +
                     Rt * (dCp / Sc) * (dHp / Sh));
}

struct Rgb {
    double r;
    double g;
    double b;
};

double encode(double c)
{
    c = std::max(0.0, std::min(1.0, c));
    if (c <= 0.0031308) {
        return 255.0 * 12.92 * c;
    }
    return 255.0 * (1.055 * std::pow(c, 1.0 / 2.4) - 0.055);
}

// Machado 2009 deuteranopia, severity 1, in linear RGB.
Rgb deuteranopia(int r, int g, int b)
{
    const double R = linear_channel(r);
    const double G = linear_channel(g);
    const double B = linear_channel(b);
    const double rr = 0.367322 * R + 0.860646 * G - 0.227968 * B;
    const double gg = 0.280085 * R + 0.672501 * G + 0.047413 * B;
    const double bb = -0.011820 * R + 0.042940 * G + 0.968881 * B;
    return Rgb{encode(rr), encode(gg), encode(bb)};
}

bool same_dash(const GraphSeriesStyle &left, const GraphSeriesStyle &right)
{
    return left.dash_on == right.dash_on && left.dash_off == right.dash_off;
}

int check_palette(bool dark)
{
    for (int i = 0; i < kGraphSeriesCount; ++i) {
        for (int j = i + 1; j < kGraphSeriesCount; ++j) {
            const GraphSeriesStyle &left = kGraphSeries[i];
            const GraphSeriesStyle &right = kGraphSeries[j];
            const int lr = dark ? left.dark_r : left.light_r;
            const int lg = dark ? left.dark_g : left.light_g;
            const int lb = dark ? left.dark_b : left.light_b;
            const int rr = dark ? right.dark_r : right.light_r;
            const int rg = dark ? right.dark_g : right.light_g;
            const int rb = dark ? right.dark_b : right.light_b;
            const double distance = ciede2000(rgb_to_lab(lr, lg, lb), rgb_to_lab(rr, rg, rb));
            if (!(distance > 20.0)) {
                std::cerr << (dark ? "dark " : "light ") << left.name << " vs " << right.name << " CIEDE2000 "
                          << distance << "\n";
                return 1;
            }
            const Rgb ld = deuteranopia(lr, lg, lb);
            const Rgb rd = deuteranopia(rr, rg, rb);
            const double confused =
                ciede2000(rgb_to_lab(static_cast<int>(std::lround(ld.r)), static_cast<int>(std::lround(ld.g)),
                                     static_cast<int>(std::lround(ld.b))),
                          rgb_to_lab(static_cast<int>(std::lround(rd.r)), static_cast<int>(std::lround(rd.g)),
                                     static_cast<int>(std::lround(rd.b))));
            if (confused <= 20.0 && same_dash(left, right)) {
                std::cerr << "deuteranopia " << left.name << " vs " << right.name << " " << confused
                          << " share a dash\n";
                return 1;
            }
            if (same_dash(left, right)) {
                std::cerr << left.name << " and " << right.name << " use the same dash\n";
                return 1;
            }
        }
    }
    return 0;
}

std::string read_file(const std::string &path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

int main()
{
    const double sample = ciede2000(Lab{50.0, 2.6772, -79.7751}, Lab{50.0, 0.0, -82.7485});
    if (std::fabs(sample - 2.0425) > 0.001) {
        std::cerr << "CIEDE2000 self-check " << sample << "\n";
        return fail(1, "CIEDE2000 does not match the Sharma sample");
    }
    if (ciede2000(Lab{40.0, 10.0, -5.0}, Lab{40.0, 10.0, -5.0}) > 0.0001) {
        return fail(1, "CIEDE2000 of one color against itself is not zero");
    }
    if (check_palette(false) != 0 || check_palette(true) != 0) {
        return fail(2, "graph series colors are too close, or a close pair shares a dash");
    }
    if (kGraphSeriesCount != 7 || std::string(kGraphSeries[1].name) != "Residential" ||
        std::string(kGraphSeries[4].name) != "Cash flow" || std::string(kGraphSeries[6].name) != "Pollution") {
        return fail(3, "the graph palette dropped a series");
    }

    if (proposed_city_filename("Tokyo", "") != "Tokyo.cty") {
        return fail(4, "Save City did not propose the city name");
    }
    if (proposed_city_filename("New City", "") != "New City.cty") {
        return fail(5, "a city name with a space lost the space");
    }
    if (proposed_city_filename("Harbor/Town", "") != "Harbor_Town.cty") {
        std::cerr << proposed_city_filename("Harbor/Town", "") << "\n";
        return fail(6, "a slash in the city name was not replaced");
    }
    if (proposed_city_filename("  ...  ", "") != "city.cty") {
        return fail(7, "a blank city name did not fall back to city.cty");
    }
    if (proposed_city_filename("Tokyo", "/tmp/maps/harbor.cty") != "harbor.cty") {
        return fail(8, "Save As ignored the current file name");
    }
    if (proposed_city_filename("Tokyo", "/tmp/maps/harbor") != "harbor.cty") {
        return fail(9, "a current file without .cty did not gain the suffix");
    }
    if (proposed_city_filename("foo.cty", "") != "foo.cty") {
        return fail(10, "the city name gained a second .cty suffix");
    }

    char home_template[] = "/tmp/lunduke-round7-home-XXXXXX";
    char *home = mkdtemp(home_template);
    if (home == nullptr) {
        return fail(11, "could not make a home directory");
    }
    setenv("HOME", home, 1);
    const std::string folder = std::string(home) + "/maps";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    remember_city_folder(folder + "/Tokyo.cty");
    if (remembered_city_folder() != folder) {
        std::cerr << "remembered '" << remembered_city_folder() << "'\n";
        return fail(12, "Save City did not remember the folder");
    }
    const std::string flag = std::string(home) + "/.config/lunduke-city/last-folder";
    if (flag.find("/xfce") != std::string::npos) {
        return fail(13, "the remembered folder was stored under XFCE");
    }
    remember_city_folder("/tmp/xfce4/maps/city.cty");
    if (remembered_city_folder() != folder) {
        return fail(14, "an XFCE path replaced the remembered folder");
    }
    setenv("HOME", "/tmp/xfce-home-round7", 1);
    remember_city_folder(folder + "/Tokyo.cty");
    if (std::filesystem::exists("/tmp/xfce-home-round7", ec)) {
        return fail(15, "the folder memory wrote an XFCE home");
    }
    setenv("HOME", home, 1);

#ifndef LUNDUKE_CITY_SOURCE_DIR
    return fail(16, "the packaging files were not located");
#else
    const std::string root = LUNDUKE_CITY_SOURCE_DIR;
    const std::string desktop = read_file(root + "/data/lunduke-city.desktop");
    const std::string mime = read_file(root + "/data/lunduke-city.xml");
    const std::string triggers = read_file(root + "/debian/lunduke-city.triggers");
    const std::string deb = read_file(root + "/packaging/build-deb.sh");
    const std::string changelog = read_file(root + "/debian/changelog");
    if (desktop.find("Exec=lunduke-city %f") == std::string::npos ||
        desktop.find("MimeType=application/x-lunduke-city;") == std::string::npos) {
        return fail(17, "the desktop file does not open a .cty");
    }
    if (mime.find("application/x-lunduke-city") == std::string::npos ||
        mime.find("*.cty") == std::string::npos) {
        return fail(18, "the shared-mime-info file does not describe .cty");
    }
    if (triggers.find("/usr/share/mime/packages") == std::string::npos) {
        return fail(19, "the MIME trigger is missing");
    }
    if (deb.find("VERSION=\"0.9.1-1\"") == std::string::npos ||
        deb.find("shared-mime-info") == std::string::npos ||
        deb.find("lunduke-city.triggers") == std::string::npos) {
        return fail(20, "the package build does not ship 0.9.1-1 with the MIME trigger");
    }
    if (changelog.find("lunduke-city (0.9.1-1)") == std::string::npos ||
        changelog.find("Phil <phil@brushpad.local>") == std::string::npos) {
        return fail(21, "the changelog is not 0.9.1-1");
    }
#endif
    return 0;
}

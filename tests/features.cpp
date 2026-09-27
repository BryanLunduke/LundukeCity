// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless checks for the 0.2 bridge: budget funding, overlay samples,
// sprite state, and sound startup. No window is opened.

#include "city_session.hpp"
#include "sound_player.hpp"

#include "micropolis.h"

#include <cstdio>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

} // namespace

int main()
{
    CitySession session;
    session.new_city("Feature Check", 42);

    session.set_road_funding(40);
    session.set_police_funding(0);
    session.set_fire_funding(100);
    session.set_tax(9);
    const CitySession::BudgetBook book = session.budget();
    if (book.road_percent != 40 || book.police_percent != 0 || book.fire_percent != 100) {
        return fail(1, "funding percents were not stored");
    }
    if (book.tax_percent != 9) {
        return fail(2, "tax rate was not stored");
    }

    session.set_road_funding(250);
    session.set_fire_funding(-3);
    const CitySession::BudgetBook clamped = session.budget();
    if (clamped.road_percent != 100 || clamped.fire_percent != 0) {
        return fail(3, "funding percents were not clamped");
    }

    int water = 0;
    int checked = 0;
    for (int y = 0; y < CitySession::kWorldH; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int tile = session.map_value(x, y) & LOMASK;
            const bool expect = (tile >= RIVER && tile <= WATER_HIGH) || (tile >= FLOOD && tile <= LASTFLOOD);
            const int got = session.layer_value(CitySession::MapLayer::Water, x, y);
            if ((got > 0) != expect) {
                return fail(4, "water overlay does not match map tiles");
            }
            water += got;
            const int pollution = session.layer_value(CitySession::MapLayer::Pollution, x, y);
            const int crime = session.layer_value(CitySession::MapLayer::Crime, x, y);
            const int land = session.layer_value(CitySession::MapLayer::LandValue, x, y);
            const int traffic = session.layer_value(CitySession::MapLayer::Traffic, x, y);
            const int power = session.layer_value(CitySession::MapLayer::Power, x, y);
            if (pollution < 0 || crime < 0 || land < 0 || traffic < 0 || power < 0) {
                return fail(5, "overlay sample was negative");
            }
            ++checked;
        }
    }
    if (water < 50 || checked != CitySession::kWorldW * CitySession::kWorldH) {
        return fail(6, "expected a generated city to contain water");
    }

    session.set_speed(3);
    for (int i = 0; i < 8; ++i) {
        session.tick();
    }
    (void)session.layer_value(CitySession::MapLayer::LandValue, 10, 10);
    (void)session.layer_value(CitySession::MapLayer::Power, 10, 10);

    int origin_x = 0;
    int origin_y = 0;
    if (!session.stamp_neighborhood(origin_x, origin_y)) {
        return fail(7, "could not place a neighborhood on clear land");
    }
    bool residential = false;
    for (int y = 0; y < CitySession::kWorldH && !residential; ++y) {
        for (int x = 0; x < CitySession::kWorldW; ++x) {
            const int tile = session.map_value(x, y) & LOMASK;
            if (tile >= RESBASE && tile < COMBASE) {
                residential = true;
                break;
            }
        }
    }
    if (!residential) {
        return fail(8, "neighborhood did not place a residential zone");
    }

    session.place_sprite(SPRITE_AIRPLANE, origin_x + 8, origin_y + 4);
    const auto sprites = session.sprites();
    bool airplane = false;
    for (const auto &dot : sprites) {
        if (dot.type == SPRITE_AIRPLANE && dot.frame > 0 && dot.width > 0) {
            airplane = true;
        }
    }
    if (!airplane) {
        return fail(9, "airplane sprite was not reported");
    }

    SoundPlayer player;
    player.probe();
    player.set_muted(true);
    if (player.play("Siren")) {
        return fail(10, "muted player should not start playback");
    }
    player.play("not-a-real-sound");
    player.set_muted(false);
    player.play("Beep");
    return 0;
}

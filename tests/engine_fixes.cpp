// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// Headless checks for the vendored-engine edits: voteProblems() must not
// read past the problem table, and destroy() must release sprites.

#include "micropolis.h"
#include "text.h"

#include <cstdarg>
#include <cstdio>
#include <malloc.h>
#include <string>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return code;
}

} // namespace

int lunduke_city_test_vote_problems()
{
    Micropolis sim;
    // problemVotes[PROBNUM] is problemOrder[0] in this class layout.
    // The old loop bound wrote that slot.
    if (&sim.problemVotes[PROBNUM] != sim.problemOrder) {
        return fail(1, "problemOrder does not follow problemVotes");
    }

    short table[PROBNUM + 1];
    for (int i = 0; i < PROBNUM; ++i) {
        table[i] = 0;
    }
    table[CVP_CRIME] = 1000;
    // The old bound consulted this extra element. A fixed loop never reads it.
    table[PROBNUM] = 1000;

    sim.problemOrder[0] = 1234;
    sim.voteProblems(table);

    int sum = 0;
    for (int i = 0; i < PROBNUM; ++i) {
        sum += sim.problemVotes[i];
    }
    // One guaranteed crime vote every PROBNUM steps, and the loop stops
    // at 600 attempts, so crime receives 60 votes and nothing else does.
    if (sim.problemVotes[CVP_CRIME] != 60 || sum != 60) {
        std::fprintf(stderr, "crime votes %d sum %d\n", sim.problemVotes[CVP_CRIME], sum);
        return fail(2, "problem votes did not stay inside the table");
    }
    if (sim.problemOrder[0] != 1234) {
        std::fprintf(stderr, "problemOrder[0] is %d\n", sim.problemOrder[0]);
        return fail(3, "voteProblems wrote past problemVotes into problemOrder");
    }
    return 0;
}

int lunduke_city_test_destroy_sprites()
{
    {
        Micropolis sim;
        // newSprite() recycles freeSprites, so both sprites have to be
        // created before one is retired onto the free list.
        sim.makeSprite(SPRITE_BUS, 8, 8);
        sim.makeSprite(SPRITE_SHIP, 24, 24);
        SimSprite *bus = sim.globalSprites[SPRITE_BUS];
        if (bus == nullptr || sim.globalSprites[SPRITE_SHIP] == nullptr) {
            return fail(4, "bus and ship sprites were not created");
        }
        sim.destroySprite(bus);
        if (sim.freeSprites == nullptr || sim.spriteList == nullptr) {
            return fail(5, "expected a sprite on the free list and the active list");
        }
        sim.destroy();
        if (sim.spriteList != nullptr || sim.freeSprites != nullptr) {
            return fail(6, "destroy() left sprite lists allocated");
        }
        for (int i = 0; i < SPRITE_COUNT; ++i) {
            if (sim.globalSprites[i] != nullptr) {
                return fail(7, "destroy() left a global sprite pointer");
            }
        }
        sim.destroy();
    }

    auto exercise = []() {
        Micropolis sim;
        sim.makeSprite(SPRITE_AIRPLANE, 16, 16);
        sim.makeSprite(SPRITE_MONSTER, 48, 48);
        SimSprite *plane = sim.globalSprites[SPRITE_AIRPLANE];
        if (plane != nullptr) {
            sim.destroySprite(plane);
        }
    };
    for (int i = 0; i < 5; ++i) {
        exercise();
    }
    const long baseline = static_cast<long>(mallinfo2().uordblks);
    for (int i = 0; i < 40; ++i) {
        exercise();
    }
    const long grown = static_cast<long>(mallinfo2().uordblks) - baseline;
    if (grown > 2048) {
        std::fprintf(stderr, "sprite heap grew by %ld bytes\n", grown);
        return fail(8, "destroy() leaked sprites");
    }
    return 0;
}

int lunduke_city_test_residential_valve()
{
    Micropolis sim;
    // The constructor's simInit() leaves initSimLoad set, so every tick stays
    // on phase 0. Valves start at 0. The first tick bumps simCycle to an odd
    // value and skips setValves; the second tick is the even cycle that runs it.
    // History buffers are HISTORY_LENGTH bytes (240 shorts), not 240 elements
    // past that. Only the slots setValves reads need to be defined.
    sim.resHist[1] = 0;
    sim.comHist[1] = 0;
    sim.indHist[1] = 0;
    auto prime = [&sim]() {
        sim.resPop = 800;
        sim.comPop = 0;
        sim.indPop = 0;
        sim.resHist[1] = 0;
        sim.comHist[1] = 0;
        sim.indHist[1] = 0;
        sim.cityTax = 7;
        sim.gameLevel = LEVEL_EASY;
        sim.resCap = false;
        sim.comCap = false;
        sim.indCap = false;
    };
    prime();
    sim.setSpeed(3);
    sim.setPasses(1);
    sim.simTick();
    prime();
    sim.simTick();

    float residential = 0;
    float commercial = 0;
    float industrial = 0;
    sim.getDemands(&residential, &commercial, &industrial);
    // Residential demand here is a shrinking city (ratio about 0.02), so the
    // valve moves down. The old bug stored the industrial ratio (clamped to
    // 2) in resRatio and pushed residential demand up instead.
    if (residential >= 0.0f) {
        std::fprintf(stderr, "res %f com %f ind %f\n", residential, commercial, industrial);
        return fail(9, "residential demand followed the industrial ratio");
    }
    // Unclamped industrial ratio is 5, which lands on the valve cap (demand 1).
    // Clamped to indRatioMax it lands near 600/1500.
    if (industrial <= 0.0f || industrial >= 1.0f) {
        std::fprintf(stderr, "res %f com %f ind %f\n", residential, commercial, industrial);
        return fail(10, "industrial demand was not clamped to indRatioMax");
    }
    return 0;
}

int lunduke_city_test_message_sound()
{
    struct Heard {
        std::vector<std::string> names;
    } heard;
    Micropolis sim;
    sim.setEnableSound(true);
    sim.setAutoGoto(false);
    sim.callbackData = &heard;
    sim.callbackHook = [](Micropolis *, void *data, const char *name, const char *, va_list args) {
        if (name == nullptr || std::string(name) != "makeSound") {
            return;
        }
        (void)va_arg(args, char *);
        const char *sound = va_arg(args, char *);
        (void)va_arg(args, int);
        (void)va_arg(args, int);
        if (sound != nullptr && sound[0] != '\0') {
            static_cast<Heard *>(data)->names.emplace_back(sound);
        }
    };
    sim.sendMessage(MESSAGE_FIRE_REPORTED, 4, 4, true, false);
    bool siren = false;
    for (const auto &sound : heard.names) {
        if (sound == "Siren") {
            siren = true;
        }
    }
    if (!siren) {
        std::fprintf(stderr, "sounds %zu\n", heard.names.size());
        return fail(11, "a fire message did not play the siren");
    }
    return 0;
}

int main()
{
    if (const int code = lunduke_city_test_vote_problems()) {
        return code;
    }
    if (const int code = lunduke_city_test_destroy_sprites()) {
        return code;
    }
    if (const int code = lunduke_city_test_residential_valve()) {
        return code;
    }
    if (const int code = lunduke_city_test_message_sound()) {
        return code;
    }
    return 0;
}

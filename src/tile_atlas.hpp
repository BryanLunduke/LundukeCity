// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cstdint>

// Classic 16-pixel Micropolis map tiles, indexed the same way as the engine
// (low 10 bits of a map word). Loaded once from images/micropolisEngine/tiles.png.
class TileAtlas {
public:
    static constexpr int kSize = 16;

    bool loaded() const { return count_ > 0; }
    int count() const { return count_; }

    // Average color of one tile, 0..1. Out-of-range tiles report clear land.
    void average_color(int tile, double &r, double &g, double &b) const;

    // Copy one tile into a Cairo-style ARGB32 buffer (native endian, opaque).
    // dest points at the top-left pixel; stride is bytes per row.
    void blit(int tile, unsigned char *dest, int stride) const;

    bool ensure_loaded();

private:
    int count_ = 0;
    int columns_ = 16;
    // Tightly packed opaque pixels, 16*16 per tile, native ARGB32.
    // Held as bytes so the header stays free of containers' noise in TUs.
    const std::uint32_t *pixels_ = nullptr;
    const unsigned char *average_ = nullptr; // count * 3, RGB
};

TileAtlas &tile_atlas();

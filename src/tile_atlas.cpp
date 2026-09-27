// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "tile_atlas.hpp"

#include "assets.hpp"

#include <gdkmm/pixbuf.h>

#include <iostream>
#include <vector>

namespace {

std::uint32_t pack(unsigned char r, unsigned char g, unsigned char b)
{
    return (255u << 24) | (static_cast<std::uint32_t>(r) << 16) |
           (static_cast<std::uint32_t>(g) << 8) | static_cast<std::uint32_t>(b);
}

} // namespace

TileAtlas &tile_atlas()
{
    static TileAtlas atlas;
    atlas.ensure_loaded();
    return atlas;
}

bool TileAtlas::ensure_loaded()
{
    if (count_ > 0 || pixels_ != nullptr) {
        return count_ > 0;
    }
    // Sentinel so a missing file does not retry every tile.
    pixels_ = reinterpret_cast<const std::uint32_t *>(1);

    const std::string path = asset_file("images/micropolisEngine/tiles.png");
    if (path.empty()) {
        std::cerr << "Lunduke City: map tiles not found (images/micropolisEngine/tiles.png)\n";
        pixels_ = nullptr;
        return false;
    }

    Glib::RefPtr<Gdk::Pixbuf> pix;
    try {
        pix = Gdk::Pixbuf::create_from_file(path);
    } catch (const Glib::Error &err) {
        std::cerr << "Lunduke City: could not read map tiles: " << err.what() << "\n";
        pixels_ = nullptr;
        return false;
    }
    if (!pix || pix->get_bits_per_sample() != 8 || pix->get_n_channels() < 3) {
        std::cerr << "Lunduke City: map tiles are not 8-bit RGB\n";
        pixels_ = nullptr;
        return false;
    }

    columns_ = pix->get_width() / kSize;
    const int rows = pix->get_height() / kSize;
    if (columns_ < 1 || rows < 1) {
        pixels_ = nullptr;
        return false;
    }
    count_ = columns_ * rows;

    static std::vector<std::uint32_t> stored;
    static std::vector<unsigned char> averages;
    stored.assign(static_cast<std::size_t>(count_) * kSize * kSize, 0);
    averages.assign(static_cast<std::size_t>(count_) * 3, 0);

    const int channels = pix->get_n_channels();
    const int stride = pix->get_rowstride();
    const unsigned char *src = pix->get_pixels();

    for (int tile = 0; tile < count_; ++tile) {
        const int col = tile % columns_;
        const int row = tile / columns_;
        unsigned long sum_r = 0;
        unsigned long sum_g = 0;
        unsigned long sum_b = 0;
        std::uint32_t *dst = stored.data() + static_cast<std::size_t>(tile) * kSize * kSize;
        for (int y = 0; y < kSize; ++y) {
            const unsigned char *line =
                src + (row * kSize + y) * stride + col * kSize * channels;
            for (int x = 0; x < kSize; ++x) {
                const unsigned char r = line[x * channels];
                const unsigned char g = line[x * channels + 1];
                const unsigned char b = line[x * channels + 2];
                dst[y * kSize + x] = pack(r, g, b);
                sum_r += r;
                sum_g += g;
                sum_b += b;
            }
        }
        const unsigned long n = kSize * kSize;
        averages[static_cast<std::size_t>(tile) * 3] = static_cast<unsigned char>(sum_r / n);
        averages[static_cast<std::size_t>(tile) * 3 + 1] = static_cast<unsigned char>(sum_g / n);
        averages[static_cast<std::size_t>(tile) * 3 + 2] = static_cast<unsigned char>(sum_b / n);
    }

    pixels_ = stored.data();
    average_ = averages.data();
    return true;
}

void TileAtlas::average_color(int tile, double &r, double &g, double &b) const
{
    if (average_ == nullptr || tile < 0 || tile >= count_) {
        r = 0.80;
        g = 0.50;
        b = 0.40;
        return;
    }
    const unsigned char *p = average_ + static_cast<std::size_t>(tile) * 3;
    r = p[0] / 255.0;
    g = p[1] / 255.0;
    b = p[2] / 255.0;
}

void TileAtlas::blit(int tile, unsigned char *dest, int stride) const
{
    if (dest == nullptr || pixels_ == nullptr || reinterpret_cast<std::uintptr_t>(pixels_) < 16) {
        return;
    }
    if (tile < 0 || tile >= count_) {
        tile = 0;
    }
    const std::uint32_t *src = pixels_ + static_cast<std::size_t>(tile) * kSize * kSize;
    for (int y = 0; y < kSize; ++y) {
        auto *row = reinterpret_cast<std::uint32_t *>(dest + y * stride);
        for (int x = 0; x < kSize; ++x) {
            row[x] = src[y * kSize + x];
        }
    }
}

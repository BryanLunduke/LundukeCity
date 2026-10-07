// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// Shared geometry for the map and the minimap. No toolkit types, so the
// headless tests can lock the same rectangles the widgets draw.

struct AspectBox {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
};

// Largest ratio_w:ratio_h rectangle that fits in the bounds, centered.
inline AspectBox largest_aspect_box(double bounds_w, double bounds_h, double ratio_w, double ratio_h)
{
    AspectBox box;
    if (bounds_w <= 0.0 || bounds_h <= 0.0 || ratio_w <= 0.0 || ratio_h <= 0.0) {
        return box;
    }
    const double target = ratio_w / ratio_h;
    double width = bounds_w;
    double height = width / target;
    if (height > bounds_h) {
        height = bounds_h;
        width = height * target;
    }
    box.x = (bounds_w - width) / 2.0;
    box.y = (bounds_h - height) / 2.0;
    box.width = width;
    box.height = height;
    return box;
}

inline void aspect_box_fraction(const AspectBox &box, double px, double py, double &fx, double &fy)
{
    if (box.width <= 0.0 || box.height <= 0.0) {
        fx = 0.0;
        fy = 0.0;
        return;
    }
    fx = (px - box.x) / box.width;
    fy = (py - box.y) / box.height;
}

// Tile under a widget pixel. A coordinate that falls left of or above the
// shaken map is outside, including the gap an earthquake exposes.
inline bool map_tile_at(double x, double y, int shake_x, int shake_y, int tile_size, int world_w,
                        int world_h, int &tx, int &ty)
{
    if (tile_size <= 0 || world_w <= 0 || world_h <= 0) {
        return false;
    }
    const double local_x = x - static_cast<double>(shake_x);
    const double local_y = y - static_cast<double>(shake_y);
    if (local_x < 0.0 || local_y < 0.0) {
        return false;
    }
    tx = static_cast<int>(local_x) / tile_size;
    ty = static_cast<int>(local_y) / tile_size;
    return tx < world_w && ty < world_h;
}

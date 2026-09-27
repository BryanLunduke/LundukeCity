// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cairomm/surface.h>

// obj<type>-<frame>.png sprite frames. type is the engine sprite id
// (1 train .. 8 bus). frame is 1-based, matching SimSprite::frame.
Cairo::RefPtr<Cairo::ImageSurface> sprite_frame(int type, int frame);

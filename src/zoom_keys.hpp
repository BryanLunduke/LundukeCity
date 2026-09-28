// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// Keyboard zoom. Control plus the +/= key zooms in without Shift.
// The unshifted keysym on that key is GDK_KEY_equal (0x03d). The shifted
// keysym is GDK_KEY_plus (0x02b), and the keypad is GDK_KEY_KP_Add.
// Shift is not required. Ctrl-minus and keypad minus still zoom out.
enum class ZoomAction {
    None,
    In,
    Out,
};

inline ZoomAction zoom_action(unsigned keyval, unsigned modifiers)
{
    constexpr unsigned kControl = 1u << 2; // Gdk::CONTROL_MASK
    constexpr unsigned kAlt = 1u << 3;     // Gdk::MOD1_MASK
    if ((modifiers & kControl) == 0 || (modifiers & kAlt) != 0) {
        return ZoomAction::None;
    }
    switch (keyval) {
    case 0x03d:  // GDK_KEY_equal — +/= key, Shift not held
    case 0x02b:  // GDK_KEY_plus
    case 0xffab: // GDK_KEY_KP_Add
        return ZoomAction::In;
    case 0x02d:  // GDK_KEY_minus
    case 0xffad: // GDK_KEY_KP_Subtract
        return ZoomAction::Out;
    default:
        return ZoomAction::None;
    }
}

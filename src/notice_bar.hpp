// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

// The message bar shows an engine notice for a few seconds, then the tool
// hint. A later event with the same words is a new serial, so it shows again.
struct NoticeDecision {
    bool show_notice = false;
    bool show_hint = false;
};

inline NoticeDecision decide_notice(int event_serial, int shown_serial, bool expired, bool query_pinned)
{
    NoticeDecision decision;
    if (event_serial != 0 && event_serial != shown_serial) {
        decision.show_notice = true;
        return decision;
    }
    if (!query_pinned && expired) {
        decision.show_hint = true;
    }
    return decision;
}

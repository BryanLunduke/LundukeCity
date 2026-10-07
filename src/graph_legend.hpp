// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <algorithm>
#include <string>

// Residential history stores resPop/8. City population counts that sample,
// and the commercial and industrial samples, as people with *160:
// (resPop + (comPop + indPop) * 8) * 20 == (resSample + comSample + indSample) * 160.
inline constexpr long kHistoryPeoplePerSample = 160;

enum class GraphLegendKind {
    Population,
    People,
    CashFlow,
    Level,
};

inline std::string grouped_number(long value)
{
    const bool neg = value < 0;
    unsigned long mag = static_cast<unsigned long>(neg ? -value : value);
    const std::string digits = std::to_string(mag);
    std::string text;
    int count = 0;
    for (int i = static_cast<int>(digits.size()) - 1; i >= 0; --i) {
        if (count > 0 && count % 3 == 0) {
            text.push_back(',');
        }
        text.push_back(digits[static_cast<std::size_t>(i)]);
        ++count;
    }
    std::reverse(text.begin(), text.end());
    if (neg) {
        text.insert(text.begin(), '-');
    }
    return text;
}

inline std::string graph_money(long value)
{
    const bool neg = value < 0;
    return (neg ? "-$" : "$") + grouped_number(neg ? -value : value);
}

// Legend text for one history series. People samples are scaled to residents.
// Crime and pollution stay 0-255 and are labeled as levels. Cash flow uses
// the engine's (sample - 128) * 20 encoding.
inline std::string graph_legend_caption(const char *name, GraphLegendKind kind, long sample)
{
    std::string caption = name != nullptr ? name : "";
    caption += ": ";
    switch (kind) {
    case GraphLegendKind::Population:
        caption += grouped_number(sample);
        break;
    case GraphLegendKind::People:
        caption += grouped_number(sample * kHistoryPeoplePerSample);
        caption += " people";
        break;
    case GraphLegendKind::CashFlow:
        caption += graph_money(static_cast<long>(sample - 128) * 20L);
        break;
    case GraphLegendKind::Level:
        caption += std::to_string(sample);
        caption += " level";
        break;
    }
    return caption;
}

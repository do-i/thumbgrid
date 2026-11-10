// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Light/dark and contrast decisions shared by the theme, thumbnails and dialogs.

#pragma once

#include <QColor>

namespace tg::colorutil {

// HSV value at or below this counts as a dark background.
inline constexpr float kDarkValueMax = 0.45f;
// Perceived luma above this wants dark text on top (glyph on a chip or badge).
inline constexpr qreal kLightLumaMin = 0.55;

inline bool isDark(const QColor &c) { return c.valueF() <= kDarkValueMax; }

// Rec. 601 luma of the RGB channels, 0..1.
inline qreal luma(const QColor &c) {
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
}

// Near-black on light backgrounds, near-white on dark ones.
inline QColor contrastGlyph(const QColor &background) {
    return luma(background) > kLightLumaMin ? QColor(25, 25, 25) : QColor(245, 245, 245);
}

} // namespace tg::colorutil

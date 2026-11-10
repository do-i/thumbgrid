// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Mouse wheel versus trackpad detection shared by the viewer and thumbnail view.

#pragma once

#include <QElapsedTimer>
#include <QWheelEvent>
#include <cstdlib>

namespace tg::wheel {

// A wheel sends angleDelta = 120*m (pixelDelta is 0 or varies by platform); a
// trackpad sends smaller or arbitrarily scaled deltas. Without scroll phases
// (X11, Windows) guess: whole notches, no trackpad event in the last 250 ms.
inline constexpr int kNotchDelta = 120;
inline constexpr int kHalfNotchDelta = 60;
inline constexpr int kTrackpadQuietMs = 250;

// lastTouchpadScroll: restarted by the caller on every trackpad event.
inline bool isWheelEvent(const QWheelEvent *event, bool trackpadDetection, bool wayland,
                         const QElapsedTimer &lastTouchpadScroll) {
    if(!trackpadDetection)
        return true;
    if(wayland) // scroll phases are available
        return event->phase() == Qt::NoScrollPhase;
    const int angleDelta = event->angleDelta().y();
    return angleDelta && (std::abs(angleDelta) >= kNotchDelta && !(angleDelta % kHalfNotchDelta))
           && lastTouchpadScroll.elapsed() > kTrackpadQuietMs;
}

} // namespace tg::wheel

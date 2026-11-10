// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Human-readable byte sizes shared by the status bar and the folder presenter.

#pragma once

#include <QString>
#include <QtGlobal>

namespace tg::bytesize {

// "512 Bytes", "1.50 KiB" ... "2.00 TiB" (binary units, two decimals).
inline QString format(qint64 bytes) {
    if(bytes < 1024)
        return QString::number(bytes) + " Bytes";

    static constexpr const char *kUnits[] = {"KiB", "MiB", "GiB", "TiB"};
    constexpr int kLastUnit = 3;
    double value = bytes / 1024.0;
    int unit = 0;
    while(value >= 1024.0 && unit < kLastUnit) {
        value /= 1024.0;
        unit++;
    }
    return QString::number(value, 'f', 2) + " " + QLatin1String(kUnits[unit]);
}

} // namespace tg::bytesize

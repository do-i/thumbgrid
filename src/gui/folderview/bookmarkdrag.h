// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// A bookmark dragged to a new place carries its path under a private type, never as file URLs,
// so the tree and the grid take it for foreign data and only the bookmarks panel accepts it.

#pragma once

#include <QLatin1String>
#include <QMimeData>

inline constexpr char kBookmarkMime[] = "application/x-thumbgrid-bookmark";

inline bool isBookmarkDrag(const QMimeData *mimeData) {
    return mimeData->hasFormat(QLatin1String(kBookmarkMime));
}

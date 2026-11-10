// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Starts another instance of the app on a folder. The launcher can be swapped so tests spawn nothing.

#pragma once

#include <functional>
#include <QString>

namespace NewWindow {

using Launcher = std::function<void(const QString &path)>;

// opens path in a new app window
void open(const QString &path);
// replaces the launcher; an empty one restores the default
void setLauncher(Launcher launcher);

} // namespace NewWindow

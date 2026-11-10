// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).

#include "newwindow.h"

#include <QCoreApplication>
#include <QProcess>

namespace {
NewWindow::Launcher &launcher() {
    static NewWindow::Launcher instance;
    return instance;
}
}

void NewWindow::open(const QString &path) {
    if(launcher()) {
        launcher()(path);
        return;
    }
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {path});
}

void NewWindow::setLauncher(Launcher newLauncher) {
    launcher() = std::move(newLauncher);
}

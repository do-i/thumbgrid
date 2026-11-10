// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Esc closes each popup and overlay through the one dismiss path, without also
// running the global Esc binding. Keys carry no native scancode here.

#include "support/thumbgrid_test_support.h"

#include <QSignalSpy>

#include "gui/contextmenu.h"
#include "gui/customwidgets/floatingwidgetcontainer.h"
#include "gui/folderview/fvoptionspopup.h"
#include "gui/folderview/gridcontextmenu.h"
#include "gui/overlays/copyoverlay.h"
#include "gui/overlays/renameoverlay.h"

class EscClosesEveryPopupTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void escClosesPopupMenus();
    void escClosesOverlays();

private:
    std::unique_ptr<QSignalSpy> mEscAction;
};

namespace {

void verifyEscHides(QWidget *widget, const char *name) {
    QTRY_VERIFY2(widget->isVisible(), name);
    QTest::keyClick(widget, Qt::Key_Escape);
    QVERIFY2(!widget->isVisible(), name);
}

} // namespace

void EscClosesEveryPopupTest::initTestCase() {
    QCOMPARE(actionManager->actionForShortcut(MODE_GLOBAL, "Esc"), QStringLiteral("closeFullScreenOrExit"));
    mEscAction = std::make_unique<QSignalSpy>(actionManager, &ActionManager::closeFullScreenOrExit);
}

void EscClosesEveryPopupTest::escClosesPopupMenus() {
    ContextMenu viewerMenu;
    viewerMenu.showAt(QPoint(10, 10));
    verifyEscHides(&viewerMenu, "viewer context menu");

    GridContextMenu gridMenu;
    gridMenu.showAt(QPoint(10, 10));
    verifyEscHides(&gridMenu, "grid context menu");

    FVOptionsPopup options;
    options.showAt(QPoint(10, 10));
    verifyEscHides(&options, "grid options popup");

    QCOMPARE(mEscAction->count(), 0);
}

void EscClosesEveryPopupTest::escClosesOverlays() {
    FloatingWidgetContainer container;
    container.resize(800, 600);
    container.show();
    QVERIFY(QTest::qWaitForWindowExposed(&container));

    RenameOverlay rename(&container);
    rename.setName("a.png");
    rename.show();
    verifyEscHides(&rename, "rename overlay");

    CopyOverlay copy(&container);
    copy.show();
    verifyEscHides(&copy, "copy overlay");

    QCOMPARE(mEscAction->count(), 0);
}

TG_BEHAVIOR_TEST_MAIN(EscClosesEveryPopupTest)

#include "test_esc_closes_every_popup.moc"

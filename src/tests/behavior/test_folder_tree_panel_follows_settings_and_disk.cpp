// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// The places panel only reacts to settings that changed, the folder icon is
// re-tinted only for a new colour, and folders created on disk appear in the tree.

#include "support/tree_test_fixture.h"

#include <QSignalSpy>
#include <QSplitter>

class FolderTreePanelFollowsSettingsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void anUnrelatedSettingKeepsThePanelWidth();
    void theFolderIconIsRetintedOnlyForANewColour();
    void aFolderCreatedOnDiskAppearsInTheTree();

private:
    tgtest::TreeFixture mFixture;
};

void FolderTreePanelFollowsSettingsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
}

void FolderTreePanelFollowsSettingsTest::anUnrelatedSettingKeepsThePanelWidth() {
    auto splitter = mFixture.folderView()->findChild<QSplitter *>("splitter");
    QVERIFY2(splitter != nullptr, "The places panel splitter should exist.");
    QVERIFY2(settings->placesPanelWidth() != 400, "The stored width should differ from the test width.");
    const int total = splitter->sizes().at(0) + splitter->sizes().at(1);
    splitter->setSizes({400, total - 400});
    QCOMPARE(splitter->sizes().at(0), 400);

    settings->setFolderViewShowInfo(!settings->folderViewShowInfo());
    settings->sendChangeNotification();
    QCOMPARE(splitter->sizes().at(0), 400);

    settings->setPlacesPanelWidth(300);
    settings->sendChangeNotification();
    QCOMPARE(splitter->sizes().at(0), 300);
}

void FolderTreePanelFollowsSettingsTest::theFolderIconIsRetintedOnlyForANewColour() {
    QSignalSpy retinted(mFixture.model(), &FileSystemModelCustom::folderIconChanged);
    settings->setFolderViewShowInfo(!settings->folderViewShowInfo());
    settings->sendChangeNotification();
    QCOMPARE(retinted.count(), 0);

    ColorScheme colors = settings->colorScheme();
    colors.icons = QColor(255, 0, 0);
    settings->setColorScheme(colors);
    settings->sendChangeNotification();
    QCOMPARE(retinted.count(), 1);
    const QImage icon = mFixture.index("a").data(Qt::DecorationRole).value<QPixmap>().toImage();
    const QPoint center(icon.width() / 2, icon.height() / 2);
    QCOMPARE(icon.pixelColor(center).rgb(), QColor(255, 0, 0).rgb());
}

void FolderTreePanelFollowsSettingsTest::aFolderCreatedOnDiskAppearsInTheTree() {
    const QModelIndex b = mFixture.index("b");
    mFixture.tree()->expand(b);
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path("b")), "The tree should list folder b.");
    QCOMPARE(mFixture.model()->rowCount(b), 0);

    QVERIFY2(QDir(mFixture.path("b")).mkdir("fresh"), "The new folder should be created.");

    QTRY_COMPARE(mFixture.model()->rowCount(b), 1);
    QCOMPARE(mFixture.model()->index(0, 0, b).data().toString(), QString("fresh"));
}

TG_BEHAVIOR_TEST_MAIN(FolderTreePanelFollowsSettingsTest)

#include "test_folder_tree_panel_follows_settings_and_disk.moc"

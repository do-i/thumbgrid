// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Hidden folders follow the "show hidden files" setting; a row's tooltip is its full path.

#include "support/tree_test_fixture.h"

class FolderTreeHiddenAndTooltipsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void hiddenFoldersFollowTheShowHiddenSetting();

private:
    void setShowHidden(bool on);
    // index(path) resolves filtered rows too, so look at the listed children
    bool hiddenRowListed() const {
        const QModelIndex c = mFixture.index("c");
        for(int row = 0; row < mFixture.model()->rowCount(c); ++row) {
            if(mFixture.model()->index(row, 0, c).data().toString() == ".hidden")
                return true;
        }
        return false;
    }
    tgtest::TreeFixture mFixture;
};

void FolderTreeHiddenAndTooltipsTest::initTestCase() {
    QVERIFY2(mFixture.build(), "The fixture folders should be created.");
    QVERIFY2(mFixture.start(), "The app should open the home folder.");
    QVERIFY2(mFixture.waitForTreeLoaded(mFixture.path()), "The tree should list the home folder.");
}

void FolderTreeHiddenAndTooltipsTest::setShowHidden(bool on) {
    settings->setShowHiddenFiles(on);
    settings->sendChangeNotification();
}

void FolderTreeHiddenAndTooltipsTest::hiddenFoldersFollowTheShowHiddenSetting() {
    setShowHidden(false);
    mFixture.tree()->expand(mFixture.index("c"));
    QTRY_VERIFY2(mFixture.model()->rowCount(mFixture.index("c")) == 0 && !mFixture.model()->canFetchMore(mFixture.index("c")),
                 "Folder c should be listed without its hidden child.");
    QVERIFY2(!hiddenRowListed(), "A hidden folder should be absent while the setting is off.");

    setShowHidden(true);
    QTRY_VERIFY2(mFixture.model()->rowCount(mFixture.index("c")) == 1, "The hidden folder should appear when the setting is on.");
    QVERIFY(hiddenRowListed());

    setShowHidden(false);
    QTRY_COMPARE(mFixture.model()->rowCount(mFixture.index("c")), 0);
    QVERIFY(!hiddenRowListed());
}

TG_BEHAVIOR_TEST_MAIN(FolderTreeHiddenAndTooltipsTest)

#include "test_folder_tree_hidden_folders_and_path_tooltips.moc"

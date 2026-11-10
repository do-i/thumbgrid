// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Folder tree fixture: builds home/{a/{a1,a2},b,c/.hidden} under the test HOME
// (the tree root), opens the app there and waits on the tree's directory loads.

#pragma once

#include "support/thumbgrid_test_support.h"

#include <memory>

#include <QDir>
#include <QSet>

#include "core.h"
#include "gui/folderview/filesystemmodelcustom.h"
#include "gui/folderview/folderview.h"
#include "gui/folderview/treeviewcustom.h"

namespace tgtest {

class TreeFixture {
public:
    // Creates the folders plus an image in a and a/a1. Call before start().
    bool build() {
        QDir home(homePath());
        for(const char *rel : {"a/a1", "a/a2", "b", "c/.hidden"}) {
            if(!home.mkpath(rel))
                return false;
        }
        return writeImage(path("a/one.png"), Qt::red) && writeImage(path("a/a1/two.png"), Qt::blue);
    }

    // Adds rel/f000…: enough of them make the tree taller than its viewport.
    bool addFolders(const QString &rel, int count) {
        QDir home(homePath());
        for(int i = 0; i < count; ++i) {
            if(!home.mkpath(QString("%1/f%2").arg(rel).arg(i, 3, 10, QChar('0'))))
                return false;
        }
        return true;
    }

    // Opens home/rel in the app and finds the tree and its model.
    bool start(const QString &rel = QString()) {
        mCore = std::make_unique<Core>();
        if(!mCore->loadPath(path(rel)))
            return false;
        mCore->showGui();
        if(!QTest::qWaitFor([]() { return mainWindow() != nullptr && mainWindow()->isVisible(); }, 5000))
            return false;
        mTree = mainWindow()->findChild<TreeViewCustom *>("dirTreeView");
        mFolderView = mainWindow()->findChild<FolderView *>();
        mModel = mTree ? dynamic_cast<FileSystemModelCustom *>(mTree->model()) : nullptr;
        if(!mModel || !mFolderView)
            return false;
        QObject::connect(mModel, &QFileSystemModel::directoryLoaded, mModel,
                         [this](const QString &dir) { mLoaded.insert(dir); });
        return true;
    }

    static QString homePath() { return QDir::homePath(); }
    static QString path(const QString &rel = QString()) {
        return rel.isEmpty() ? homePath() : QDir::cleanPath(homePath() + "/" + rel);
    }

    Core *core() const { return mCore.get(); }
    FolderView *folderView() const { return mFolderView; }
    TreeViewCustom *tree() const { return mTree; }
    FileSystemModelCustom *model() const { return mModel; }
    QModelIndex index(const QString &rel) const { return mModel->index(path(rel)); }

    // The app navigates to home/rel, as when a folder is opened from anywhere.
    bool navigate(const QString &rel) { return mCore->loadPath(path(rel)); }

    QString currentTreePath() const { return mModel->filePath(mTree->currentIndex()); }

    // True when the row of home/rel overlaps the tree's visible area.
    bool isRowVisible(const QString &rel) const {
        const QModelIndex idx = index(rel);
        return idx.isValid() && mTree->visualRect(idx).intersects(mTree->viewport()->rect());
    }

    // Waits until the model has listed `dirPath`. Loads that finished before
    // start() connected are detected by comparing the row count with the disk.
    bool waitForTreeLoaded(const QString &dirPath, int timeoutMs = 5000) {
        const QString clean = QDir::cleanPath(dirPath);
        const int expected = QDir(clean).entryList(QDir::AllDirs | QDir::NoDotAndDotDot).count();
        auto loaded = [&]() {
            if(mLoaded.contains(clean))
                return true;
            const QModelIndex idx = mModel->index(clean);
            return idx.isValid() && !mModel->canFetchMore(idx) && expected > 0 &&
                   mModel->rowCount(idx) == expected;
        };
        if(!QTest::qWaitFor(loaded, timeoutMs))
            return false;
        QCoreApplication::processEvents(); // let the model's delayed sort run
        return true;
    }

    // Forgets earlier loads so the next wait sees a fresh directoryLoaded.
    void forgetLoads() { mLoaded.clear(); }

private:
    std::unique_ptr<Core> mCore;
    FolderView *mFolderView = nullptr;
    TreeViewCustom *mTree = nullptr;
    FileSystemModelCustom *mModel = nullptr;
    QSet<QString> mLoaded;
};

} // namespace tgtest

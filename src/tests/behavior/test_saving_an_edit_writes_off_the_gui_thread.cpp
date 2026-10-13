// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 do-i and thumbgrid contributors
// Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
//
// Saving an edit used to encode on the GUI thread: heic, avif and large png
// froze the window for seconds. The write now runs on a worker and replaces
// the file atomically (QSaveFile), with the save overlay busy meanwhile.
//
// The worker's result comes back as a queued call, so anything checked right
// after clicking Save - before the event loop runs - sees the save in flight,
// however fast the write itself is.
#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <exiv2/exiv2.hpp>
#include <memory>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/overlays/saveconfirmoverlay.h"

class SavingAnEditWritesOffTheGuiThreadTest : public QObject {
    Q_OBJECT

private slots:
    void savedFileKeepsMetadataAndPermissionsAndNoStrayFiles();
    void anEditMadeWhileSavingStaysUnsaved();
    void savingWithOtherFileTypesShownKeepsTheListIntact();
    void closingWhileSavingStillWritesTheFile();
};

namespace {

// 40x20 with a white top-left pixel: a rotation shows in the size, a 180
// degree turn in where the white pixel ends up.
QImage markedImage() {
    QImage image(40, 20, QImage::Format_RGB32);
    image.fill(QColor(60, 120, 180));
    image.setPixelColor(0, 0, Qt::white);
    return image;
}

bool isWhite(const QColor &color) {
    return color.red() > 200 && color.green() > 200 && color.blue() > 200;
}

// Core outlives neither its window (it never deletes it) nor a case, so each
// case hides its window and the next one picks the window its Core made.
struct Viewer {
    std::unique_ptr<Core> core;
    MW *window = nullptr;
    ~Viewer() {
        core.reset();
        if(window)
            window->hide();
    }
};

bool openPicture(Viewer &viewer, const QString &path) {
    const QWidgetList before = QApplication::topLevelWidgets();
    viewer.core = std::make_unique<Core>();
    if(!viewer.core->loadPath(path))
        return false;
    viewer.core->showGui();
    for(QWidget *widget : QApplication::topLevelWidgets()) {
        auto window = qobject_cast<MW *>(widget);
        if(window && !before.contains(widget))
            viewer.window = window;
    }
    return viewer.window != nullptr;
}

SaveConfirmOverlay *saveOverlay(MW *window) {
    return window->findChild<SaveConfirmOverlay *>();
}

QPushButton *overlayButton(MW *window, const char *name) {
    auto overlay = saveOverlay(window);
    return overlay ? overlay->findChild<QPushButton *>(name) : nullptr;
}

QString overlayTitle(MW *window) {
    auto overlay = saveOverlay(window);
    auto label = overlay ? overlay->findChild<QLabel *>("label") : nullptr;
    return label ? label->text() : QString();
}

bool overlayShown(MW *window) {
    auto overlay = saveOverlay(window);
    return overlay && overlay->isVisible();
}

} // namespace

void SavingAnEditWritesOffTheGuiThreadTest::savedFileKeepsMetadataAndPermissionsAndNoStrayFiles() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("a.jpg");
    QVERIFY(markedImage().save(path, "JPG", 95));
    {
        auto img = Exiv2::ImageFactory::open(path.toStdString());
        img->readMetadata();
        Exiv2::ExifData exif;
        exif["Exif.Image.Make"] = "TestCam";
        img->setExifData(exif);
        img->writeMetadata();
    }
    const auto perms = QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ReadGroup;
    QVERIFY(QFile::setPermissions(path, perms | QFileDevice::ReadOwner | QFileDevice::WriteOwner));

    Viewer viewer;
    QVERIFY2(openPicture(viewer, path), "The picture should open.");
    MW *window = viewer.window;
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    QVERIFY(actionManager->invokeAction("rotateRight"));
    QTRY_VERIFY2(overlayShown(window), "An edit should offer to save.");
    QPushButton *save = overlayButton(window, "saveButton");
    QVERIFY(save);
    save->click();

    // in flight: the overlay says so and takes no choice
    QCOMPARE(overlayTitle(window), QStringLiteral("Saving…"));
    QVERIFY2(!save->isEnabled(), "Save should be disabled while saving.");
    QVERIFY2(!overlayButton(window, "discardButton")->isEnabled(), "Discard should be disabled while saving.");
    // Discard reaching Core by another route (a shortcut) is ignored too
    emit window->discardEditsRequested();

    QTRY_VERIFY2(!overlayShown(window), "The overlay should close once the edit is saved.");
    QImage written(path);
    QCOMPARE(written.size(), QSize(20, 40));

    auto img = Exiv2::ImageFactory::open(path.toStdString());
    img->readMetadata();
    const Exiv2::ExifData &exif = img->exifData();
    auto make = exif.findKey(Exiv2::ExifKey("Exif.Image.Make"));
    QVERIFY2(make != exif.end() && make->toString() == "TestCam", "The source Exif should be carried over.");
    auto orientation = exif.findKey(Exiv2::ExifKey("Exif.Image.Orientation"));
    QVERIFY2(orientation != exif.end() && orientation->toInt64() == 1, "Orientation should be normalized.");

    QCOMPARE(QFile::permissions(path) & 0x0fff, QFileDevice::Permissions(perms));
    QCOMPARE(QDir(fixture.path()).entryList(QDir::Files | QDir::Hidden), QStringList{"a.jpg"});
}

void SavingAnEditWritesOffTheGuiThreadTest::anEditMadeWhileSavingStaysUnsaved() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("a.png");
    QVERIFY(markedImage().save(path, "PNG"));

    Viewer viewer;
    QVERIFY2(openPicture(viewer, path), "The picture should open.");
    MW *window = viewer.window;
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    QVERIFY(actionManager->invokeAction("rotateRight"));
    QTRY_VERIFY(overlayShown(window));
    overlayButton(window, "saveButton")->click();
    // a second turn before the first one has been written
    QVERIFY(actionManager->invokeAction("rotateRight"));

    QTRY_COMPARE(overlayTitle(window), QStringLiteral("Unsaved edits"));
    QVERIFY2(overlayShown(window), "The newer edit should still be unsaved.");
    QVERIFY(overlayButton(window, "saveButton")->isEnabled());
    QCOMPARE(QImage(path).size(), QSize(20, 40));

    overlayButton(window, "saveButton")->click();
    QTRY_VERIFY2(!overlayShown(window), "Saving the newer edit should close the overlay.");
    const QImage written(path);
    QCOMPARE(written.size(), QSize(40, 20));
    QVERIFY2(isWhite(written.pixelColor(39, 19)), "Both turns should be on disk.");
}

void SavingAnEditWritesOffTheGuiThreadTest::savingWithOtherFileTypesShownKeepsTheListIntact() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    const QString path = root.filePath("a.png");
    QVERIFY(markedImage().save(path, "PNG"));
    QFile other(root.filePath("b.txt"));
    QVERIFY(other.open(QIODevice::WriteOnly));
    other.write("x");
    other.close();

    settings->setShowOtherFileTypes(true);
    Viewer viewer;
    QVERIFY2(openPicture(viewer, fixture.path()), "The folder should open.");
    MW *window = viewer.window;
    QTRY_VERIFY(window->isVisible());
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid);
    QTRY_COMPARE(grid->itemCount(), 3); // "..", a.png, b.txt
    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);

    QVERIFY(actionManager->invokeAction("rotateRight"));
    QTRY_VERIFY(overlayShown(window));
    overlayButton(window, "saveButton")->click();
    QTRY_VERIFY(!overlayShown(window));
    // let the watcher deliver the temporary sibling and its rename
    QTest::qWait(500);
    QCOMPARE(grid->itemCount(), 3);

    // still the open picture: another edit lands on it
    QVERIFY(actionManager->invokeAction("rotateRight"));
    QTRY_VERIFY2(overlayShown(window), "The saved picture should still be open.");
    settings->setShowOtherFileTypes(false);
}

void SavingAnEditWritesOffTheGuiThreadTest::closingWhileSavingStillWritesTheFile() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString path = QDir(fixture.path()).filePath("big.png");
    // noise keeps png slow to encode, so the write is still running at close
    QImage image(1600, 1200, QImage::Format_RGB32);
    auto *rng = QRandomGenerator::global();
    for(int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<quint32 *>(image.scanLine(y));
        for(int x = 0; x < image.width(); ++x)
            line[x] = 0xff000000 | rng->generate();
    }
    QVERIFY(image.save(path, "PNG"));

    Viewer viewer;
    QVERIFY2(openPicture(viewer, path), "The picture should open.");
    QTRY_COMPARE(viewer.window->currentViewMode(), MODE_DOCUMENT);
    QVERIFY(actionManager->invokeAction("rotateRight"));
    QTRY_VERIFY(overlayShown(viewer.window));
    overlayButton(viewer.window, "saveButton")->click();
    viewer.core.reset();

    const QImage written(path);
    QVERIFY2(!written.isNull(), "The file should be complete, not cut off mid-write.");
    QCOMPARE(written.size(), QSize(1200, 1600));
}

TG_BEHAVIOR_TEST_MAIN(SavingAnEditWritesOffTheGuiThreadTest)

#include "test_saving_an_edit_writes_off_the_gui_thread.moc"

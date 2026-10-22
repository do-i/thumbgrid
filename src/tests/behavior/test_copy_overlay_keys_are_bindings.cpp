#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/pathselectormenuitem.h"
#include "gui/mainwindow.h"
#include "gui/overlays/copyoverlay.h"

// The Copy / Move overlay's digits used to be hard-coded ahead of every
// binding. They are now the sendToFolder1..9 actions of the "Copy / Move"
// shortcut context, which only applies while the overlay is open: the digits by
// default (Shift+digit too, as before), another key can take a folder, an
// unbound digit reaches the usual shortcuts, and each folder shows its key.

class CopyOverlayKeysAreBindingsTest : public QObject {
    Q_OBJECT

private slots:
    void folderKeysAreBindings();
};

namespace {

// Keys are named by native scancode (QTest key events carry none).
void press(QWidget *target, Qt::Key key, const QString &name, Qt::KeyboardModifiers mods = Qt::NoModifier) {
    const quint32 scanCode = inputMap->keys().key(name);
    QKeyEvent down(QEvent::KeyPress, key, mods, scanCode, 0, 0);
    QApplication::sendEvent(target, &down);
    QKeyEvent up(QEvent::KeyRelease, key, mods, scanCode, 0, 0);
    QApplication::sendEvent(target, &up);
    QCoreApplication::processEvents();
}

} // namespace

void CopyOverlayKeysAreBindingsTest::folderKeysAreBindings() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery") && root.mkpath("one") && root.mkpath("two") && root.mkpath("three"));
    QVERIFY(tgtest::writeImage(root.filePath("gallery/a.png"), Qt::darkCyan));
    const QStringList folders{root.filePath("one"), root.filePath("two"), root.filePath("three")};
    settings->setSavedPaths(folders);

    Core core;
    QVERIFY2(core.loadPath(root.filePath("gallery/a.png")), "Opening the image should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    QTRY_COMPARE(window->currentViewMode(), MODE_DOCUMENT);
    QTRY_VERIFY(window->windowTitle().contains("a.png"));

    window->triggerCopyOverlay();
    auto overlay = window->findChild<CopyOverlay *>();
    QVERIFY(overlay);
    QSignalSpy copies(overlay, &CopyOverlay::copyRequested);
    const auto pressInOverlay = [&](Qt::Key key, const QString &name, Qt::KeyboardModifiers mods = Qt::NoModifier) {
        if(!overlay->isVisible())
            window->triggerCopyOverlay();
        QTRY_VERIFY(overlay->isVisible());
        press(overlay, key, name, mods);
    };
    const auto folderLabels = [&]() {
        QStringList labels;
        for(auto item : overlay->findChildren<PathSelectorMenuItem *>())
            labels << item->shortcut();
        return labels;
    };

    // The digits are the defaults, and each folder shows its key.
    for(int slot = 1; slot <= 9; slot++)
        QCOMPARE(actionManager->actionForShortcut(MODE_COPYMOVE, QString::number(slot)),
                 QStringLiteral("sendToFolder%1").arg(slot));
    QTRY_VERIFY(overlay->isVisible());
    QCOMPARE(folderLabels().mid(0, 3), QStringList({"1", "2", "3"}));
    pressInOverlay(Qt::Key_2, "2");
    QCOMPARE(copies.count(), 1);
    QCOMPARE(copies.last().at(0).toString(), folders.at(1));
    // Shift+digit picks the folder too, as when the digits were hard-coded.
    pressInOverlay(Qt::Key_3, "3", Qt::ShiftModifier);
    QCOMPARE(copies.count(), 2);
    QCOMPARE(copies.last().at(0).toString(), folders.at(2));

    // Another key can take a folder, and the folder shows it.
    actionManager->removeShortcut(MODE_COPYMOVE, "1");
    actionManager->addShortcut(MODE_COPYMOVE, "Q", "sendToFolder1");
    overlay->hide();
    window->triggerCopyOverlay();
    QTRY_VERIFY(overlay->isVisible());
    QCOMPARE(folderLabels().first(), QStringLiteral("Q"));
    pressInOverlay(Qt::Key_Q, "Q");
    QCOMPARE(copies.count(), 3);
    QCOMPARE(copies.last().at(0).toString(), folders.at(0));

    // An unbound digit picks nothing; it reaches the usual shortcuts instead.
    pressInOverlay(Qt::Key_1, "1");
    QTest::qWait(100);
    QCOMPARE(copies.count(), 3);

    // Global bindings don't reach the overlay's own keys.
    actionManager->addShortcut(MODE_GLOBAL, "F9", "sendToFolder2");
    QVERIFY(actionManager->actionForShortcut(MODE_COPYMOVE, "F9").isEmpty());
}

TG_BEHAVIOR_TEST_MAIN(CopyOverlayKeysAreBindingsTest)

#include "test_copy_overlay_keys_are_bindings.moc"

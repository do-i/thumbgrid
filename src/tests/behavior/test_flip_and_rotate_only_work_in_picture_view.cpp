// Flip and rotate are picture-view edits. Triggered from the grid (a custom
// global binding is enough to reach them there) they used to overwrite every
// selected original after a confirmation, without ever showing the result.
// The grid now ignores them: no prompt, no write.
#include "support/thumbgrid_test_support.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class FlipAndRotateOnlyWorkInPictureViewTest : public QObject {
    Q_OBJECT

private slots:
    void gridIgnoresFlipAndRotate();
};

namespace {

QByteArray fileHash(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return {};
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
}

} // namespace

void FlipAndRotateOnlyWorkInPictureViewTest::gridIgnoresFlipAndRotate() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");
    QDir root(fixture.path());
    const QString imagePath = root.filePath("a.png");
    // not square, so a rotation would also change the size on disk
    QImage image(40, 20, QImage::Format_RGB32);
    image.fill(QColor(60, 120, 180));
    image.setPixelColor(0, 0, Qt::white); // asymmetric, so a flip changes bytes
    QVERIFY(image.save(imagePath, "PNG"));
    const QByteArray before = fileHash(imagePath);

    Core core;
    QVERIFY2(core.loadPath(fixture.path()), "Opening the gallery folder should succeed.");
    core.showGui();
    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then a.png (1).
    QTRY_COMPARE(grid->itemCount(), 2);
    grid->select(1);
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    // The old behaviour asked first; dismiss any prompt so a regression fails
    // the assertions below instead of hanging on a modal dialog.
    QStringList prompts;
    QTimer watchdog;
    connect(&watchdog, &QTimer::timeout, this, [&]() {
        if(QWidget *modal = QApplication::activeModalWidget()) {
            prompts << modal->windowTitle();
            modal->close();
        }
    });
    watchdog.start(20);

    for(const char *action : {"flipH", "flipV", "rotateLeft", "rotateRight"})
        QVERIFY2(actionManager->invokeAction(action), action);
    QTest::qWait(200);
    watchdog.stop();

    QVERIFY2(prompts.isEmpty(), qPrintable("No confirmation should be shown in the grid: " + prompts.join(", ")));
    QVERIFY2(fileHash(imagePath) == before, "The original must not be rewritten from the grid.");
    QCOMPARE(QDir(fixture.path()).entryList(QDir::Files).count(), 1);
}

TG_BEHAVIOR_TEST_MAIN(FlipAndRotateOnlyWorkInPictureViewTest)

#include "test_flip_and_rotate_only_work_in_picture_view.moc"

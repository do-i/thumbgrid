#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include "appversion.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scaler/scalerrequest.h"
#include "components/scriptmanager/scriptmanager.h"
#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "settings.h"
#include "sharedresources.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/actions.h"
#include "utils/inputmap.h"
#include "utils/script.h"

namespace {

bool writeImage(const QString &path, const QColor &color) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    return image.save(path, "PNG");
}

MW *mainWindow() {
    for(QWidget *widget : QApplication::topLevelWidgets()) {
        if(auto window = qobject_cast<MW *>(widget))
            return window;
    }
    return nullptr;
}

void initializeThumbgridForTest() {
    QCoreApplication::setOrganizationName("thumbgrid-tests");
    QCoreApplication::setOrganizationDomain("github.com/do-i/thumbgrid");
    QCoreApplication::setApplicationName("thumbgrid-tests");
    QCoreApplication::setApplicationVersion(appVersion.toString());

    qRegisterMetaType<ScalerRequest>("ScalerRequest");
    qRegisterMetaType<Script>("Script");
    qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
    qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");

    inputMap = InputMap::getInstance();
    appActions = Actions::getInstance();
    settings = Settings::getInstance();
    scriptManager = ScriptManager::getInstance();
    actionManager = ActionManager::getInstance();
    shrRes = SharedResources::getInstance();

    settings->setFirstRun(false);
    settings->setFullscreenMode(false);
    settings->setFolderViewMode(FV_EXT_FOLDERS);
    settings->setFolderViewTopBar(false);
    settings->setUseThumbnailCache(false);
}

} // namespace

class StartsInFolderAndShowsGalleryTest : public QObject {
    Q_OBJECT

private slots:
    void startsInAFolderAndShowsTheGallery();
};

void StartsInFolderAndShowsGalleryTest::startsInAFolderAndShowsTheGallery() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/child-folder"), "Gallery child folder should be created.");

    const QString galleryPath = root.filePath("gallery");
    QVERIFY2(writeImage(root.filePath("gallery/first.png"), Qt::red), "First test image should be written.");
    QVERIFY2(writeImage(root.filePath("gallery/second.png"), Qt::blue), "Second test image should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent folder tile + one child folder + two image thumbnails.
    QTRY_COMPARE(grid->itemCount(), 4);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

int main(int argc, char **argv) {
    QTemporaryDir configHome;
    QTemporaryDir cacheHome;
    if(configHome.isValid())
        qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    if(cacheHome.isValid())
        qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());

    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

    QApplication app(argc, argv);
    initializeThumbgridForTest();

    StartsInFolderAndShowsGalleryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_starts_in_folder_and_shows_gallery.moc"

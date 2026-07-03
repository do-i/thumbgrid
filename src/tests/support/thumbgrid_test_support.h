#pragma once

// Shared helpers and bootstrap for thumbgrid behavior tests.
//
// Each behavior test is its own executable (one binary per behavior) so the
// singletons set up here stay isolated between behaviors. Use the
// TG_BEHAVIOR_TEST_MAIN macro to provide a main() that:
//   - redirects config/cache to throwaway locations (no touching real files),
//   - defaults to the offscreen platform unless THUMBGRID_TEST_VISUAL is set,
//   - constructs QApplication and initializes the thumbgrid singletons.

#include <QtTest>

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryDir>

#include "appversion.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scaler/scalerrequest.h"
#include "components/scriptmanager/scriptmanager.h"
#include "gui/mainwindow.h"
#include "settings.h"
#include "sharedresources.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/actions.h"
#include "utils/inputmap.h"
#include "utils/script.h"

namespace tgtest {

// Writes a tiny solid-color PNG fixture. Returns false on failure.
inline bool writeImage(const QString &path, const QColor &color) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    return image.save(path, "PNG");
}

// Returns the live thumbgrid main window, or nullptr if none is shown yet.
inline MW *mainWindow() {
    for(QWidget *widget : QApplication::topLevelWidgets()) {
        if(auto window = qobject_cast<MW *>(widget))
            return window;
    }
    return nullptr;
}

// Initializes the global singletons the app relies on, with test-friendly
// defaults. Must be called once after QApplication is constructed.
inline void initializeThumbgrid() {
    QCoreApplication::setOrganizationName("thumbgrid-tests");
    QCoreApplication::setOrganizationDomain("github.com/do-i/thumbgrid");
    QCoreApplication::setApplicationName("thumbgrid-tests");
    QCoreApplication::setApplicationVersion(appVersion.toString());

    // Cross-platform redirect of QSettings/standard paths to a throwaway area.
    QStandardPaths::setTestModeEnabled(true);

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

} // namespace tgtest

// Provides a main() for a single behavior test class.
#define TG_BEHAVIOR_TEST_MAIN(TestClass)                                        \
    int main(int argc, char **argv) {                                           \
        QTemporaryDir testHome;                                                 \
        QTemporaryDir configHome;                                               \
        QTemporaryDir cacheHome;                                                \
        /* QStandardPaths test mode resolves under $HOME/.qttest, which is      \
           shared between test binaries; isolate HOME so state written by one   \
           behavior test (e.g. shortcut migration) cannot leak into another. */ \
        if(testHome.isValid()) {                                                \
            qputenv("HOME", testHome.path().toUtf8());                          \
            qputenv("USERPROFILE", testHome.path().toUtf8());                   \
        }                                                                       \
        if(configHome.isValid())                                                \
            qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());             \
        if(cacheHome.isValid())                                                 \
            qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());               \
        if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") &&                    \
           !qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))                 \
            qputenv("QT_QPA_PLATFORM", "offscreen");                            \
        qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");                            \
        QApplication app(argc, argv);                                           \
        tgtest::initializeThumbgrid();                                          \
        TestClass tc;                                                           \
        return QTest::qExec(&tc, argc, argv);                                   \
    }

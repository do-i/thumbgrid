#include <QApplication>
#include <QCommandLineParser>
#include <QEvent>
#include <QIcon>

#include "appversion.h"
#include "platform/platformdesktop.h"
#include "settings.h"
#include "components/actionmanager/actionmanager.h"
#include "utils/inputmap.h"
#include "utils/actions.h"
#include "utils/cmdoptionsrunner.h"
#include "sharedresources.h"
#include "components/storeddata/storeddataregistry.h"
#include "core.h"

#ifdef __APPLE__
#include "macosapplication.h"
#endif

//------------------------------------------------------------------------------
void saveSettings() {
    // Runs at exit, after Core has written its final session state (saved
    // paths, geometry) - so an on-exit clear cannot be re-written by shutdown
    // code. Must precede the Settings teardown that flushes the config.
    StoredData::clearStoresMarkedForExit();
    delete settings;
}
//------------------------------------------------------------------------------
QDataStream& operator<<(QDataStream& out, const Script& v) {
    out << v.command << v.blocking;
    return out;
}
//------------------------------------------------------------------------------
QDataStream& operator>>(QDataStream& in, Script& v) {
    in >> v.command;
    in >> v.blocking;
    return in;
}
//------------------------------------------------------------------------------
int main(int argc, char *argv[]) {

    PlatformDesktop::prepareApplicationEnvironment();

    // for hidpi testing
    //qputenv("QT_SCALE_FACTOR","1.5");
    //qputenv("QT_SCREEN_SCALE_FACTORS", "1;1.7");

    // do we still need this?
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR","0");

#if (QT_VERSION_MAJOR == 5)
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    PlatformDesktop::applyHighDpiPolicy();


#ifdef __APPLE__
    MacOSApplication a(argc, argv);
#else
    QApplication a(argc, argv);
#endif
    PlatformDesktop::applyApplicationStyle(&a);

    QCoreApplication::setOrganizationName("thumbgrid");
    QCoreApplication::setOrganizationDomain("github.com/do-i/thumbgrid");
    QCoreApplication::setApplicationName("thumbgrid");
    QCoreApplication::setApplicationVersion(appVersion.toString());
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QGuiApplication::setDesktopFileName(QCoreApplication::applicationName() + ".desktop");
    // Populate _NET_WM_ICON so taskbars/docks get the icon directly from the
    // window, instead of relying on WM_CLASS<->desktop-file association.
    QApplication::setWindowIcon(QIcon::fromTheme(QCoreApplication::applicationName()));

    // needed for mpv
#ifndef _MSC_VER
    setlocale(LC_NUMERIC, "C");
#endif

#ifdef __GLIBC__
    // default value of 128k causes memory fragmentation issues
    mallopt(M_MMAP_THRESHOLD, 64000);
#endif

    // use custom types in signals
    qRegisterMetaType<ScalerRequest>("ScalerRequest");
    qRegisterMetaType<Script>("Script");
    qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
    qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qRegisterMetaTypeStreamOperators<Script>("Script");
#endif

    // globals
    inputMap = InputMap::getInstance();
    appActions = Actions::getInstance();
    settings = Settings::getInstance();
    scriptManager = ScriptManager::getInstance();
    actionManager = ActionManager::getInstance();
    shrRes = SharedResources::getInstance();

    atexit(saveSettings);

// parse args ------------------------------------------------------------------
    QCommandLineParser parser;
    QString appDescription = qApp->applicationName() + " - Fast and configurable image viewer.";
    appDescription.append("\nVersion: " + qApp->applicationVersion());
    appDescription.append("\nLicense: GNU GPLv3");
    parser.setApplicationDescription(appDescription);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("path", QCoreApplication::translate("main", "File or directory path."));
    parser.addOptions({
        {"gen-thumbs",
            QCoreApplication::translate("main", "Generate all thumbnails for directory."),
            QCoreApplication::translate("main", "directory-path")},
        {"gen-thumbs-size",
            QCoreApplication::translate("main", "Thumbnail size. Current size is used if not specified."),
            QCoreApplication::translate("main", "thumbnail-size")},
        {"build-options",
            QCoreApplication::translate("main", "Show build options.")},
    });
    parser.process(a);

    if(parser.isSet("build-options")) {
        CmdOptionsRunner r;
        QTimer::singleShot(0, &r, &CmdOptionsRunner::showBuildOptions);
        return a.exec();
    } else if(parser.isSet("gen-thumbs")) {
        int size = settings->folderViewIconSize();
        if(parser.isSet("gen-thumbs-size"))
            size = parser.value("gen-thumbs-size").toInt();

        CmdOptionsRunner r;
        QTimer::singleShot(0, &r,
                           std::bind(&CmdOptionsRunner::generateThumbs, &r, parser.value("gen-thumbs"), size));
        return a.exec();
    }

// -----------------------------------------------------------------------------

    Core core;

#ifdef __APPLE__
    QObject::connect(&a, &MacOSApplication::fileOpened, &core, &Core::loadPath);
#endif

    if(parser.positionalArguments().count())
        core.loadPath(parser.positionalArguments().at(0));
    else if(settings->defaultViewMode() == MODE_FOLDERVIEW)
        core.loadPath(QDir::homePath());

    // wait for event queue to catch up before showing window
    // this avoids white background flicker on windows (or not?)
    // FIXME: re-entrancy hazard (processEvents)
    qApp->processEvents();

    core.showGui();
    return a.exec();
}

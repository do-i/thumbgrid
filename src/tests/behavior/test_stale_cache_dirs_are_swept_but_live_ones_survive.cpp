// Settings::setupCache() sweeps stale "videothumb-*" and "export-*" scratch
// directories out of the cache root on startup (src/settings.cpp, near the
// QLockFile-guarded loop right after the writable-cache fallback). Because
// this app enforces no single instance - a file manager spawns one process
// per opened image - the sweep must never delete a scratch dir a live
// sibling instance is still using. It is guarded by two filters:
//   * an age floor (dirs younger than 5 minutes are left alone), and
//   * a QLockFile probe on "<dir>/.tg-lock" (a locked dir is left alone).
//
// The sweep runs once, inside setupCache(), the first time
// Settings::getInstance() is called. So this test cannot use the shared
// TG_BEHAVIOR_TEST_MAIN macro: that macro's main() calls
// tgtest::initializeThumbgrid(), which calls Settings::getInstance() itself,
// which would run the sweep before any fixture directory exists. Instead
// this file writes its own main() - mirroring test_settings_migration_gates.cpp
// - that redirects HOME/XDG_CONFIG_HOME/XDG_CACHE_HOME exactly as the shared
// macro does, but defers Settings::getInstance() until after the fixture
// directories are in place under the real cache root
// (PlatformDesktop::defaultCacheDirectory()).

#include "appversion.h"
#include "settings.h"
#include "platform/platformdesktop.h"
#include "components/actionmanager/actionmanager.h"
#include "components/scaler/scalerrequest.h"
#include "sourcecontainers/image.h"
#include "sourcecontainers/thumbnail.h"
#include "utils/script.h"
#include "utils/actions.h"
#include "utils/inputmap.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <utime.h>
#endif

namespace {

bool fail(const QString &message) {
    qWarning().noquote() << message;
    return false;
}

bool require(bool condition, const QString &message) {
    return condition || fail(message);
}

// Pushes a directory's mtime back so it clears setupCache()'s 5-minute age
// floor. Qt has no portable API for backdating a directory's mtime, so this
// drops to POSIX utime(). Gated to Q_OS_UNIX per the project's Linux-first
// stance; verified below with QFileInfo rather than trusted blindly.
bool backdateMtime(const QString &path, int secondsAgo) {
#ifdef Q_OS_UNIX
    const struct utimbuf times {
        ::time(nullptr) - secondsAgo,
        ::time(nullptr) - secondsAgo,
    };
    return ::utime(path.toLocal8Bit().constData(), &times) == 0;
#else
    Q_UNUSED(path);
    Q_UNUSED(secondsAgo);
    return false;
#endif
}

bool makeDir(const QDir &root, const QString &name) {
    return root.mkpath(name);
}

// Exercises the sweep in setupCache() and checks all five outcomes it must
// produce in a single pass: two stale/unlocked scratch dirs removed, a fresh
// scratch dir spared by the age floor, a stale-but-locked export dir spared
// by the lock probe (the case that protects a live sibling instance), and an
// unrelated directory left untouched because it matches neither prefix.
bool staleCacheDirsAreSweptButLiveOnesSurvive() {
    const QString cacheRoot = PlatformDesktop::defaultCacheDirectory();
    QDir root(cacheRoot);
    if(!require(root.mkpath(QStringLiteral(".")), "Failed to create the cache root fixture directory."))
        return false;

    const QString oldVideothumb = QStringLiteral("videothumb-oldXXXX");
    const QString oldExport = QStringLiteral("export-oldXXXX");
    const QString freshVideothumb = QStringLiteral("videothumb-freshXXXX");
    const QString liveExport = QStringLiteral("export-liveXXXX");
    const QString unrelated = QStringLiteral("thumbnails");

    if(!require(makeDir(root, oldVideothumb), "Failed to create the old videothumb fixture.") ||
       !require(makeDir(root, oldExport), "Failed to create the old export fixture.") ||
       !require(makeDir(root, freshVideothumb), "Failed to create the fresh videothumb fixture.") ||
       !require(makeDir(root, liveExport), "Failed to create the live export fixture.") ||
       !require(makeDir(root, unrelated), "Failed to create the unrelated fixture."))
        return false;

    const QString oldVideothumbPath = root.filePath(oldVideothumb);
    const QString oldExportPath = root.filePath(oldExport);
    const QString freshVideothumbPath = root.filePath(freshVideothumb);
    const QString liveExportPath = root.filePath(liveExport);
    const QString unrelatedPath = root.filePath(unrelated);

    // Hold the lock on the "live" export dir ourselves, standing in for a
    // sibling instance that is still using it. QLockFile reclaims a lock
    // whose owning pid is dead, but this pid (ours) is alive for the whole
    // test, so the sweep's tryLock(0) must fail and skip the directory.
    //
    // This has to happen *before* backdating liveExportPath's mtime below:
    // acquiring a QLockFile writes a helper file into the target directory
    // (to stash pid/hostname/appname), which bumps the directory's own
    // mtime as a side effect. Backdating first and locking second would
    // silently undo the backdate and let this case pass for the wrong
    // reason - the age floor, not the lock probe actually under test.
    QLockFile liveLock(liveExportPath + QStringLiteral("/.tg-lock"));
    liveLock.setStaleLockTime(0);
    if(!require(liveLock.tryLock(0), "Test process failed to acquire its own simulated live-session lock."))
        return false;

    // Backdate the two dirs that should be swept, and the now-locked dir
    // (which must survive on the lock alone, not on the age floor). The
    // fresh dir and the unrelated dir are left at their just-created mtime.
    const int oneHourAgo = 3600;
    if(!require(backdateMtime(oldVideothumbPath, oneHourAgo), "Failed to backdate the old videothumb fixture's mtime.") ||
       !require(backdateMtime(oldExportPath, oneHourAgo), "Failed to backdate the old export fixture's mtime.") ||
       !require(backdateMtime(liveExportPath, oneHourAgo), "Failed to backdate the live export fixture's mtime."))
        return false;

    const QDateTime staleBefore = QDateTime::currentDateTime().addSecs(-300);
    if(!require(QFileInfo(oldVideothumbPath).lastModified() < staleBefore,
                "Backdating the old videothumb fixture did not take effect on this filesystem.") ||
       !require(QFileInfo(oldExportPath).lastModified() < staleBefore,
                "Backdating the old export fixture did not take effect on this filesystem.") ||
       !require(QFileInfo(liveExportPath).lastModified() < staleBefore,
                "Backdating the live export fixture did not take effect on this filesystem.") ||
       !require(QFileInfo(freshVideothumbPath).lastModified() >= staleBefore,
                "The fresh videothumb fixture unexpectedly reads as stale."))
        return false;

    // First touch of Settings::getInstance() in this process: runs
    // setupCache(), which runs the sweep exactly once.
    Settings::getInstance();

    // Each check is evaluated independently (no short-circuiting) so a
    // failing run reports every violated expectation, not just the first.
    const bool sweptOldVideothumb = require(!QFileInfo::exists(oldVideothumbPath), "Old, unlocked videothumb-* dir should have been swept.");
    const bool sweptOldExport = require(!QFileInfo::exists(oldExportPath), "Old, unlocked export-* dir should have been swept.");
    const bool freshSurvived = require(QFileInfo::exists(freshVideothumbPath), "Fresh videothumb-* dir should survive the age floor.");
    const bool lockedSurvived = require(QFileInfo::exists(liveExportPath), "Locked export-* dir should survive the lock probe (protects a live sibling instance).");
    const bool unrelatedSurvived = require(QFileInfo::exists(unrelatedPath), "Unrelated dir matching neither prefix should never be touched.");
    const bool ok = sweptOldVideothumb && sweptOldExport && freshSurvived && lockedSurvived && unrelatedSurvived;

    liveLock.unlock();
    return ok;
}

} // namespace

int main(int argc, char **argv) {
    QTemporaryDir testHome;
    QTemporaryDir configHome;
    QTemporaryDir cacheHome;
    // Same redirect TG_BEHAVIOR_TEST_MAIN performs (see
    // src/tests/support/thumbgrid_test_support.h): the qttest home dir is
    // shared across behavior test binaries, so HOME must be isolated here
    // too, even though this file cannot use the shared macro itself (see the
    // file header comment for why).
    if(testHome.isValid()) {
        qputenv("HOME", testHome.path().toUtf8());
        qputenv("USERPROFILE", testHome.path().toUtf8());
    }
    if(configHome.isValid())
        qputenv("XDG_CONFIG_HOME", configHome.path().toUtf8());
    if(cacheHome.isValid())
        qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "0");

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("thumbgrid-tests");
    QCoreApplication::setOrganizationDomain("github.com/do-i/thumbgrid");
    QCoreApplication::setApplicationName("thumbgrid-tests");
    QCoreApplication::setApplicationVersion(appVersion.toString());
    QStandardPaths::setTestModeEnabled(true);
    qRegisterMetaType<ScalerRequest>("ScalerRequest");
    qRegisterMetaType<Script>("Script");
    qRegisterMetaType<std::shared_ptr<Image>>("std::shared_ptr<Image>");
    qRegisterMetaType<std::shared_ptr<Thumbnail>>("std::shared_ptr<Thumbnail>");
    inputMap = InputMap::getInstance();
    appActions = Actions::getInstance();

    return staleCacheDirsAreSweptButLiveOnesSurvive() ? 0 : 1;
}

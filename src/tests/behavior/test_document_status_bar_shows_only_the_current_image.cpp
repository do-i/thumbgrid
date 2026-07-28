#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>

#include "core.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "gui/panels/infobar/infobarproxy.h"
#include "gui/viewers/viewerwidget.h"

// The document view's status footer used to carry the grid's line: an object
// count, a selection count, the file size in brackets, *then* the image, with
// the size repeated, the bit depth glued onto the resolution and the zoom in
// parentheses. Almost none of that describes the picture on screen.
//
// It shows the current image and nothing else now: file name, file size, WxH,
// zoom as a plain multiplier (1.00x is 100%).

namespace {

constexpr int kViewportW = 900;
constexpr int kViewportH = 500;
constexpr int kImageW    = 640;
constexpr int kImageH    = 480;

const QString kFileName = QStringLiteral("photo.png");

// The windowed status footer. FolderViewProxy owns a second InfoBarProxy for the
// grid's own footer, so pick by the accessible name InfoBarProxy sets on itself;
// the folder one renames itself to "FolderViewStatusFooter".
InfoBarProxy *documentStatusFooter(MW *window) {
    for(InfoBarProxy *bar : window->findChildren<InfoBarProxy *>())
        if(bar->accessibleName() == QStringLiteral("InfoBarProxy"))
            return bar;
    return nullptr;
}

// InfoBar puts the status line in its "path" label.
QString footerText(MW *window) {
    InfoBarProxy *footer = documentStatusFooter(window);
    if(!footer)
        return QString();
    auto label = footer->findChild<QLabel *>(QStringLiteral("path"));
    return label ? label->text() : QString();
}

} // namespace

class DocumentStatusBarTest : public QObject {
    Q_OBJECT

private:
    // Opens one image in document view and returns the window. nullptr on any
    // setup failure.
    MW *openImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core);

private slots:
    void initTestCase();
    void theFooterListsNameSizeBytesAndZoomOnly();
    void theZoomFieldFollowsTheViewer();
};

void DocumentStatusBarTest::initTestCase() {
    // The footer under test is the windowed one; without this it never inits.
    settings->setInfoBarWindowed(true);
    // A resized window would fight the fixed viewport these checks assume.
    settings->setAutoResizeWindow(false);
}

MW *DocumentStatusBarTest::openImage(QTemporaryDir &fixture, std::unique_ptr<Core> &core) {
    if(!fixture.isValid())
        return nullptr;
    QDir root(fixture.path());
    if(!root.mkpath("gallery"))
        return nullptr;
    const QString galleryPath = root.filePath("gallery");

    QImage image(kImageW, kImageH, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    if(!image.save(QDir(galleryPath).filePath(kFileName), "PNG"))
        return nullptr;

    // Earlier slots' windows can still be alive (they are deleted via
    // deleteLater), so wait for the one this Core creates rather than the first
    // one that happens to exist.
    QSet<QWidget *> preexisting;
    for(QWidget *w : QApplication::topLevelWidgets())
        if(qobject_cast<MW *>(w))
            preexisting.insert(w);

    core.reset(new Core());
    if(!core->loadPath(galleryPath))
        return nullptr;
    core->showGui();

    MW *window = nullptr;
    for(int i = 0; i < 200 && !window; ++i) {
        for(QWidget *w : QApplication::topLevelWidgets()) {
            if(preexisting.contains(w))
                continue;
            if(auto candidate = qobject_cast<MW *>(w)) {
                window = candidate;
                break;
            }
        }
        QCoreApplication::processEvents();
    }
    if(!window)
        return nullptr;

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    if(!grid)
        return nullptr;
    // The image, plus the entry that sits ahead of it.
    for(int i = 0; i < 200 && grid->itemCount() < 2; ++i)
        QCoreApplication::processEvents();

    window->resize(kViewportW, kViewportH);
    QTest::qWait(150);

    grid->select(1);
    QTest::keyClick(grid, Qt::Key_Return);
    for(int i = 0; i < 200 && window->currentViewMode() != MODE_DOCUMENT; ++i)
        QCoreApplication::processEvents();
    QTest::qWait(150);
    return window;
}

void DocumentStatusBarTest::theFooterListsNameSizeBytesAndZoomOnly() {
    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    MW *window = openImage(fixture, core);
    QVERIFY2(window, "The image should open in document view.");

    const QString text = footerText(window);
    QVERIFY2(!text.isEmpty(), "The document footer should carry the image line.");

    // Name, file size, resolution, zoom - in that order, and nothing else.
    const QRegularExpression expected(
        QStringLiteral("^photo\\.png\\s+[0-9]+\\.?[0-9]*\\s(Bytes|KiB|MiB|GiB|TiB)\\s+640x480\\s+[0-9]+\\.[0-9]{2}x$"));
    QVERIFY2(expected.match(text).hasMatch(),
             qPrintable(QStringLiteral("Unexpected footer line: \"%1\"").arg(text)));

    // The specific things that used to be in there.
    QVERIFY2(!text.contains(QStringLiteral("object(s)")),
             "The grid's object counts do not belong to a single open image.");
    QVERIFY2(!text.contains(u'(') && !text.contains(u')'),
             "The zoom level is written plainly, without parentheses.");
    QVERIFY2(!text.contains(QStringLiteral("640x480x")),
             "The bit depth should not be glued onto the resolution.");

    // The file size appears once, not twice.
    const QString onDisk = QFileInfo(QDir(fixture.path()).filePath("gallery/" + kFileName)).size() < 1024
                               ? QStringLiteral("Bytes")
                               : QStringLiteral("iB");
    QCOMPARE(text.count(onDisk), 1);
}

// The zoom field is live: it tracks the viewer, it is not a snapshot taken when
// the image was opened.
void DocumentStatusBarTest::theZoomFieldFollowsTheViewer() {
    QTemporaryDir fixture;
    std::unique_ptr<Core> core;
    MW *window = openImage(fixture, core);
    QVERIFY2(window, "The image should open in document view.");

    const QString before = footerText(window);
    auto viewer = window->findChild<ViewerWidget *>();
    QVERIFY2(viewer, "The document view should own a viewer.");

    viewer->zoomIn();
    QTest::qWait(150);

    const QString after = footerText(window);
    QVERIFY2(after != before,
             qPrintable(QStringLiteral("Zooming should change the footer, still: \"%1\"").arg(after)));
    QVERIFY2(after.endsWith(u'x'), "The zoom level stays the last field, as a multiplier.");
}

TG_BEHAVIOR_TEST_MAIN(DocumentStatusBarTest)

#include "test_document_status_bar_shows_only_the_current_image.moc"

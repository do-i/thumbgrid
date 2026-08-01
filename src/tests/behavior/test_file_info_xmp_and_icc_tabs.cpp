#include "support/thumbgrid_test_support.h"

#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QTabWidget>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

class FileInfoXmpAndIccTabsTest : public QObject {
    Q_OBJECT

private slots:
    void xmpAndIccTabsFollowWhatTheFormatCanCarry();
};

namespace {

// Same trick test_file_info_folder_and_exif_states.cpp uses: rows for every
// tab are children of the dialog regardless of which tab is current, so
// scoping the search to a specific tab widget (rather than the whole dialog)
// is what keeps a same-named row on two different tabs from colliding.
QString rowValueByName(QWidget *root, const QString &rowName) {
    for(EntryInfoItem *item : root->findChildren<EntryInfoItem *>()) {
        const QList<QLabel *> labels = item->findChildren<QLabel *>();
        if(labels.size() < 2 || labels.at(0)->text() != rowName)
            continue;
        const QString tip = labels.at(1)->toolTip();
        return tip.isEmpty() ? labels.at(1)->text() : tip;
    }
    return QString();
}

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

// A PNG carrying an sRGB ICC profile (Qt's PNG writer embeds ~480 bytes when
// the QImage carries a colour space) and, optionally, an Xmp.dc.title written
// straight through exiv2 - which leaves the ICC chunk untouched.
bool writePngWithIccAndOptionalXmp(const QString &path, const QColor &color,
                                    const QString &xmpTitle) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    image.setColorSpace(QColorSpace::SRgb);
    if(!image.save(path, "PNG"))
        return false;
    if(xmpTitle.isEmpty())
        return true;
    try {
        std::unique_ptr<Exiv2::Image> exivImage = Exiv2::ImageFactory::open(path.toStdString());
        if(!exivImage.get())
            return false;
        exivImage->readMetadata();
        Exiv2::XmpData &xmpData = exivImage->xmpData();
        xmpData["Xmp.dc.title"] = xmpTitle.toStdString();
        exivImage->writeMetadata();
    } catch(Exiv2::Error &) {
        return false;
    }
    return true;
}

// A jpeg carrying an sRGB ICC profile plus one Exif tag and one XMP tag, so a
// single file exercises all three tabs at once.
bool writeJpegWithExifXmpAndIcc(const QString &path, const QColor &color,
                                 const std::string &make, const QString &xmpTitle) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    image.setColorSpace(QColorSpace::SRgb);
    if(!image.save(path, "JPG"))
        return false;
    try {
        std::unique_ptr<Exiv2::Image> exivImage = Exiv2::ImageFactory::open(path.toStdString());
        if(!exivImage.get())
            return false;
        exivImage->readMetadata();
        Exiv2::ExifData &exifData = exivImage->exifData();
        exifData["Exif.Image.Make"] = make;
        Exiv2::XmpData &xmpData = exivImage->xmpData();
        xmpData["Xmp.dc.title"] = xmpTitle.toStdString();
        exivImage->writeMetadata();
    } catch(Exiv2::Error &) {
        return false;
    }
    return true;
}

} // namespace

void FileInfoXmpAndIccTabsTest::xmpAndIccTabsFollowWhatTheFormatCanCarry() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery/sub"), "sub folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString xmpIccPngPath = root.filePath("gallery/a_xmp_icc.png");
    const QString exifXmpIccJpgPath = root.filePath("gallery/b_exif_xmp_icc.jpg");
    const QString iccOnlyPngPath = root.filePath("gallery/c_icc_only.png");
    const QString textPath = root.filePath("gallery/note.txt");

    QVERIFY2(writePngWithIccAndOptionalXmp(xmpIccPngPath, Qt::red, "Sunrise over the bay"),
             "PNG with XMP and ICC should be written.");
    QVERIFY2(writeJpegWithExifXmpAndIcc(exifXmpIccJpgPath, Qt::green, "TestCam", "Evening market"),
             "JPEG with Exif, XMP and ICC should be written.");
    QVERIFY2(writePngWithIccAndOptionalXmp(iccOnlyPngPath, Qt::blue, QString()),
             "PNG with ICC only should be written.");
    {
        QFile note(textPath);
        QVERIFY(note.open(QIODevice::WriteOnly));
        note.write("just text");
    }

    // note.txt needs "other file types" switched on to appear in the grid at
    // all; images and folders show regardless.
    settings->setShowOtherFileTypes(true);

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    // Parent ".." (0), then dirs sorted (sub=1), then files sorted
    // (a_xmp_icc.png=2, b_exif_xmp_icc.jpg=3, c_icc_only.png=4, note.txt=5).
    QTRY_COMPARE(grid->itemCount(), 6);
    const int subDir = 1;
    const int xmpIccPng = 2;
    const int exifXmpIccJpg = 3;
    const int iccOnlyPng = 4;
    const int textFile = 5;

    // --- Open on the PNG carrying XMP + ICC: this is the headline behaviour -
    // a PNG has no Exif tab (Exiv2 cannot write Exif into a PNG) but both XMP
    // and ICC are formats a PNG can carry, so both are shown. ---
    grid->select(xmpIccPng);
    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");

    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(xmpIccPngPath).absoluteFilePath());

    const int exifIndex = tabIndexByText(dialog->tabs(), "EXIF");
    const int xmpIndex = tabIndexByText(dialog->tabs(), "XMP");
    const int iccIndex = tabIndexByText(dialog->tabs(), "ICC");
    const int generalIndex = tabIndexByText(dialog->tabs(), "General");
    QVERIFY2(exifIndex >= 0 && xmpIndex >= 0 && iccIndex >= 0 && generalIndex >= 0,
             "General, EXIF, XMP and ICC tabs should all exist.");

    QVERIFY2(!dialog->tabs()->isTabVisible(exifIndex),
             "EXIF tab should be hidden for a png, which Exiv2 cannot write Exif into.");
    QVERIFY2(dialog->tabs()->isTabVisible(xmpIndex), "XMP tab should be shown for a png carrying XMP.");
    QVERIFY2(dialog->tabs()->isTabVisible(iccIndex), "ICC tab should be shown for a png carrying an ICC profile.");

    QVERIFY2(!dialog->xmpPlaceholder()->isVisibleTo(dialog->xmpTab()),
             "the XMP placeholder should stay hidden once real rows are showing");
    QVERIFY2(!dialog->iccPlaceholder()->isVisibleTo(dialog->iccTab()),
             "the ICC placeholder should stay hidden once real rows are showing");
    QCOMPARE(dialog->iccRows().size(), 4);
    QCOMPARE(rowValueByName(dialog->iccTab(), "Profile"), QStringLiteral("sRGB"));
    // dc:title is a curated key now, so it appears as the editable "Title"
    // field rather than as a row in the read-only property dump - showing it in
    // both places would be the same property twice.
    EntryInfoItem *titleRow = dialog->editableXmpRow(QStringLiteral("Xmp.dc.title"));
    QVERIFY2(titleRow != nullptr, "a writable png should offer the curated XMP Title field");
    QVERIFY2(titleRow->currentValue().contains("Sunrise over the bay"),
             "the XMP Title field should carry the value written into the file");

    // --- Live-follow to the jpeg carrying all three kinds of metadata: EXIF,
    // XMP and ICC should all be visible together. ---
    grid->select(exifXmpIccJpg);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(exifXmpIccJpgPath).absoluteFilePath());
    QTRY_VERIFY2(dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be shown for a tagged jpeg.");
    QVERIFY2(dialog->tabs()->isTabVisible(xmpIndex), "XMP tab should be shown for a jpeg carrying XMP.");
    QVERIFY2(dialog->tabs()->isTabVisible(iccIndex), "ICC tab should be shown for a jpeg carrying an ICC profile.");
    QTRY_COMPARE(rowValueByName(dialog, "Make"), QStringLiteral("TestCam"));
    QCOMPARE(rowValueByName(dialog->iccTab(), "Profile"), QStringLiteral("sRGB"));
    EntryInfoItem *jpegTitleRow = dialog->editableXmpRow(QStringLiteral("Xmp.dc.title"));
    QVERIFY2(jpegTitleRow != nullptr, "a writable jpeg should offer the curated XMP Title field");
    QVERIFY2(jpegTitleRow->currentValue().contains("Evening market"),
             "the XMP Title field should update to the newly selected file's value");

    // --- Live-follow to a png with an ICC profile but no XMP: the XMP tab
    // stays open (gated on what the format can carry, not on what this file
    // happens to hold) and shows its placeholder instead of a blank pane. ---
    grid->select(iccOnlyPng);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(iccOnlyPngPath).absoluteFilePath());
    QTRY_VERIFY2(!dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be hidden for a png.");
    QVERIFY2(dialog->tabs()->isTabVisible(xmpIndex),
             "XMP tab should stay shown for a png even when it carries no XMP today.");
    QVERIFY2(dialog->tabs()->isTabVisible(iccIndex), "ICC tab should be shown for a png carrying an ICC profile.");
    QCOMPARE(dialog->xmpRows().size(), 0);
    // The placeholder existed to avoid a blank pane. A writable png now offers
    // the curated fields instead, which is a better answer to "this file has no
    // XMP" than a label saying so - it is also where the first property gets
    // typed in.
    QVERIFY2(!dialog->xmpPlaceholder()->isVisibleTo(dialog->xmpTab()),
             "a writable png offers XMP fields to type into rather than an empty-state label.");
    QVERIFY2(dialog->editableXmpRow(QStringLiteral("Xmp.dc.title")) != nullptr,
             "the curated XMP fields should be offered for a writable png carrying no XMP yet.");
    QVERIFY2(dialog->editableXmpRow(QStringLiteral("Xmp.dc.title"))->currentValue().isEmpty(),
             "and they should be empty, since the file carries no XMP.");
    QCOMPARE(rowValueByName(dialog->iccTab(), "Profile"), QStringLiteral("sRGB"));

    // --- Switch to the ICC tab, then retarget to a text file: EXIF, XMP and
    // ICC should all close, and the dialog must fall back to General rather
    // than leaving a hidden tab selected. ---
    dialog->tabs()->setCurrentIndex(iccIndex);
    QCOMPARE(dialog->tabs()->currentIndex(), iccIndex);

    grid->select(textFile);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(textPath).absoluteFilePath());
    QTRY_VERIFY2(!dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be hidden for a text file.");
    QVERIFY2(!dialog->tabs()->isTabVisible(xmpIndex), "XMP tab should be hidden for a text file.");
    QVERIFY2(!dialog->tabs()->isTabVisible(iccIndex), "ICC tab should be hidden for a text file.");
    QTRY_COMPARE(dialog->tabs()->currentIndex(), generalIndex);

    // --- Retarget from a png (XMP + ICC open) to a folder while the ICC tab
    // is current: falls back to General, never left on a now-hidden tab. ---
    grid->select(xmpIccPng);
    QTRY_VERIFY2(dialog->tabs()->isTabVisible(iccIndex), "ICC tab should reopen for the png.");
    dialog->tabs()->setCurrentIndex(iccIndex);
    QCOMPARE(dialog->tabs()->currentIndex(), iccIndex);

    grid->select(subDir);
    QTRY_COMPARE(rowValueByName(dialog, "Path"), QFileInfo(root.filePath("gallery/sub")).absoluteFilePath());
    QVERIFY2(!dialog->tabs()->isTabVisible(exifIndex), "EXIF tab should be hidden for a folder.");
    QVERIFY2(!dialog->tabs()->isTabVisible(xmpIndex), "XMP tab should be hidden for a folder.");
    QVERIFY2(!dialog->tabs()->isTabVisible(iccIndex), "ICC tab should be hidden for a folder.");
    QTRY_COMPARE(dialog->tabs()->currentIndex(), generalIndex);

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoXmpAndIccTabsTest)

#include "test_file_info_xmp_and_icc_tabs.moc"

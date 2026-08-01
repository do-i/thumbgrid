// The XMP tab's editing surface: the custom-property grid, the remove toggle,
// and the one visibility rule the three scoped removal buttons share.
//
// The value handling underneath is covered by test_xmp_editing_round_trips.cpp;
// what is asserted here is the part that only exists as widgets - that there is
// always somewhere to type, that a delete affordance never appears uninvited,
// and that a greyed button means something different from an absent one.
//
// One long test method rather than several, matching the other File info
// behaviour tests: the window is built once by a single Core and retargeted
// through each case, which is also how it is used.
//
// See docs/2026-08-01-001 §5 (grid), §6 (removals), §7 (visibility).

#include "support/thumbgrid_test_support.h"

#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"

class FileInfoXmpGridTest : public QObject {
    Q_OBJECT

private slots:
    void theXmpTabEditsCustomPropertiesAndScopesItsButtons();
};

namespace {

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

bool writeJpeg(const QString &path, const QColor &color) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(color);
    image.setColorSpace(QColorSpace::SRgb);
    return image.save(path, "jpeg");
}

// Seeds two custom properties in a namespace exiv2 does not know, plus one
// standard property, so both groups have something in them. The prefix is
// declared in the file itself, which is what makes it writable later without a
// namespace URI.
bool seedXmp(const QString &path) {
    try {
        Exiv2::XmpProperties::registerNs("http://thumbgrid.example/gridtest/", "tggrid");
        auto image = Exiv2::ImageFactory::open(path.toStdString());
        image->readMetadata();
        Exiv2::XmpData &xmp = image->xmpData();
        xmp["Xmp.photoshop.Headline"] = "a standard property";
        xmp["Xmp.tggrid.Zebra"] = "last alphabetically";
        xmp["Xmp.tggrid.Alpha"] = "first alphabetically";
        image->writeMetadata();
        return true;
    } catch(...) {
        return false;
    }
}

} // namespace

void FileInfoXmpGridTest::theXmpTabEditsCustomPropertiesAndScopesItsButtons() {
    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    QDir root(fixture.path());
    QVERIFY(root.mkpath("gallery"));
    const QString galleryPath = root.filePath("gallery");
    const QString barePath = root.filePath("gallery/a_bare.jpg");
    const QString seededPath = root.filePath("gallery/b_seeded.jpg");
    const QString lockedPath = root.filePath("gallery/c_locked.jpg");
    QVERIFY(writeJpeg(barePath, Qt::darkRed));
    QVERIFY(writeJpeg(seededPath, Qt::darkGreen));
    QVERIFY(seedXmp(seededPath));
    QVERIFY(writeJpeg(lockedPath, Qt::darkYellow));
    QVERIFY(seedXmp(lockedPath));
    QVERIFY(QFile::setPermissions(lockedPath, QFile::ReadOwner));

    Core core;
    QVERIFY(core.loadPath(galleryPath));
    core.showGui();
    QTRY_VERIFY(tgtest::mainWindow() != nullptr);
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY(window->isVisible());
    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY(grid != nullptr);
    QTRY_COMPARE(grid->itemCount(), 4); // "..", a_bare, b_seeded, c_locked
    const int bare = 1, seeded = 2, locked = 3;

    grid->select(bare);
    QVERIFY(actionManager->invokeAction("toggleImageInfo"));
    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->isVisible());
    const int xmpIndex = tabIndexByText(dialog->tabs(), QStringLiteral("XMP"));
    const int iccIndex = tabIndexByText(dialog->tabs(), QStringLiteral("ICC"));
    QVERIFY(xmpIndex >= 0 && iccIndex >= 0);
    dialog->tabs()->setCurrentIndex(xmpIndex);

    // --- A file carrying no XMP still offers somewhere to type. An empty tab
    // would leave nowhere to start, which is precisely the file where someone
    // wants to add the first property. ---
    QTRY_COMPARE(dialog->customXmpRows().size(), 1);
    QVERIFY2(dialog->customXmpRows().first()->isBlank(),
             "the only grid row on a bare file should be the blank affordance row");
    QVERIFY2(dialog->editableXmpRow(QStringLiteral("Xmp.dc.title")) != nullptr,
             "the curated fields should be offered even with no XMP present");
    QVERIFY2(!dialog->removeXmpButton()->isVisibleTo(dialog),
             "with no XMP there is nothing to remove, so the button is hidden entirely");

    // --- A standard key typed into the custom grid is refused with a reason.
    // Its declared type is what a free-text cell would corrupt. ---
    EntryInfoItem *blank = dialog->customXmpRows().first();
    QVERIFY(blank->nameEditor() != nullptr && blank->valueEditor() != nullptr);
    blank->nameEditor()->setText(QStringLiteral("Xmp.dc.title"));
    blank->valueEditor()->setText(QStringLiteral("should not land here"));
    blank->commitEdit();
    QVERIFY2(dialog->xmpNotice()->isVisibleTo(dialog->xmpTab()),
             "a standard key in the grid should say why it cannot be edited there");
    QCOMPARE(DocumentInfo(barePath).getCustomXmpTags().size(), 0);

    // --- An unknown prefix asks for a namespace URI instead of throwing at the
    // write, and stays uncommitted until one is given. ---
    blank->nameEditor()->setText(QStringLiteral("Xmp.tgnew.Thing"));
    blank->valueEditor()->setText(QStringLiteral("needs a namespace"));
    blank->commitEdit();
    QVERIFY2(blank->extraEditor() != nullptr,
             "an unknown prefix should reveal the namespace URI cell");
    QCOMPARE(DocumentInfo(barePath).getCustomXmpTags().size(), 0);

    // Filling only the URI has to count as a change, or the very edit that makes
    // the row valid would be swallowed by an unchanged name and value.
    blank->extraEditor()->setText(QStringLiteral("http://thumbgrid.example/new/1.0/"));
    blank->commitEdit();
    QTRY_COMPARE(DocumentInfo(barePath).getCustomXmpTags().size(), 1);
    QCOMPARE(DocumentInfo(barePath).getCustomXmpTags().value(QStringLiteral("Xmp.tgnew.Thing")),
             QStringLiteral("needs a namespace"));

    // The write retargets the window, so the grid is rebuilt: the property is a
    // real row now, with a fresh blank appended beneath it.
    QTRY_COMPARE(dialog->customXmpRows().size(), 2);
    QVERIFY2(dialog->customXmpRows().last()->isBlank(),
             "filling the trailing row must append a new blank one beneath it");
    QCOMPARE(dialog->customXmpRows().first()->currentName(), QStringLiteral("Xmp.tgnew.Thing"));
    QVERIFY2(dialog->removeXmpButton()->isVisibleTo(dialog),
             "now that the file carries XMP, its removal button appears without a reopen");

    // --- A value with no key has nothing to be written under. ---
    EntryInfoItem *stillBlank = dialog->customXmpRows().last();
    stillBlank->valueEditor()->setText(QStringLiteral("orphaned value"));
    stillBlank->commitEdit();
    QVERIFY(dialog->xmpNotice()->isVisibleTo(dialog->xmpTab()));
    QCOMPARE(DocumentInfo(barePath).getCustomXmpTags().size(), 1);

    // --- Both groups appear on the seeded file, each sorted by key: exiv2's own
    // iteration order is neither grouped nor alphabetical. ---
    grid->select(seeded);
    QTRY_COMPARE(dialog->customXmpRows().size(), 3); // Alpha, Zebra, blank
    QCOMPARE(dialog->customXmpRows().at(0)->currentName(), QStringLiteral("Xmp.tggrid.Alpha"));
    QCOMPARE(dialog->customXmpRows().at(1)->currentName(), QStringLiteral("Xmp.tggrid.Zebra"));
    QVERIFY(dialog->customXmpRows().at(2)->isBlank());
    QCOMPARE(dialog->xmpRows().size(), 1);
    QVERIFY2(dialog->xmpRows().first()->currentName() == QStringLiteral("Xmp.photoshop.Headline"),
             "a standard property stays in the read-only list, not the editable grid");

    // --- The remove toggle: off by default, and it never touches rows whose
    // identity is not their key. ---
    QVERIFY2(!dialog->removeToggleButton()->isChecked(), "the remove toggle starts off");
    for(EntryInfoItem *row : dialog->customXmpRows())
        QVERIFY2(row->removeButton() == nullptr, "no row offers an x until the toggle is on");

    dialog->removeToggleButton()->setChecked(true);
    QVERIFY(dialog->customXmpRows().at(0)->removeButton() != nullptr);
    QVERIFY(dialog->customXmpRows().at(1)->removeButton() != nullptr);
    QVERIFY2(dialog->customXmpRows().at(2)->removeButton() == nullptr,
             "the trailing blank row has nothing to remove, so it gains no x");

    dialog->customXmpRows().at(0)->removeButton()->click();
    QTRY_COMPARE(DocumentInfo(seededPath).getCustomXmpTags().size(), 1);
    QVERIFY(DocumentInfo(seededPath).getCustomXmpTags().contains(QStringLiteral("Xmp.tggrid.Zebra")));
    QVERIFY2(DocumentInfo(seededPath).getXmpTags().contains(QStringLiteral("Xmp.photoshop.Headline")),
             "removing a custom property must not touch a standard one");

    // --- A delete mode must not follow the user onto the next file. ---
    dialog->removeToggleButton()->setChecked(true);
    grid->select(locked);
    QTRY_VERIFY(!dialog->customXmpRows().isEmpty());
    QVERIFY2(!dialog->removeToggleButton()->isChecked(),
             "the remove toggle must reset when the window retargets");
    QVERIFY2(dialog->customXmpRows().at(0)->removeButton() == nullptr,
             "and no row should still be showing an x afterwards");

    // --- The second axis: something to remove, but the file cannot be written.
    // Shown and greyed, which is the one thing greying now means. ---
    QVERIFY2(dialog->removeXmpButton()->isVisibleTo(dialog),
             "a read-only file still carries XMP, so the button is shown");
    QVERIFY2(!dialog->removeXmpButton()->isEnabled(),
             "...but greyed, because this file cannot be written");
    QVERIFY2(dialog->xmpNotice()->isVisibleTo(dialog->xmpTab()),
             "and the tab should say why its fields cannot be edited");

    // --- Each button is scoped to its own tab; none leaks onto another. ---
    dialog->tabs()->setCurrentIndex(iccIndex);
    QVERIFY2(dialog->removeIccButton()->isVisibleTo(dialog),
             "the ICC tab offers the profile removal for a file that carries one");
    QVERIFY2(!dialog->removeXmpButton()->isVisibleTo(dialog),
             "the XMP removal must not appear on the ICC tab");
    QVERIFY2(!dialog->removeExifButton()->isVisibleTo(dialog), "nor the Exif one");

    // Restore permissions so the temporary directory can be cleaned up.
    QVERIFY(QFile::setPermissions(lockedPath, QFile::ReadOwner | QFile::WriteOwner));
}

TG_BEHAVIOR_TEST_MAIN(FileInfoXmpGridTest)

#include "test_file_info_xmp_grid.moc"

// The Date/Time field in the File info window's EXIF tab offers a calendar
// drop-down, and still takes typed text - the picker is an addition to the
// field, not a replacement for it. Both routes end in the same Exif string, and
// blanking the field (which no date widget can express) still removes the tag.

#include "support/thumbgrid_test_support.h"

#include <QAction>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>

#include <exiv2/exiv2.hpp>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/datetimepickerpopup.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"

class FileInfoDatePickerTest : public QObject {
    Q_OBJECT

private slots:
    void theCalendarFillsTheDateFieldAndTypingStillWorks();
};

namespace {

const QString kDate = QStringLiteral("Exif.Image.DateTime");

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

// Opens the calendar the way the user does: the trailing action inside the
// field.
DateTimePickerPopup *openPicker(FileInfoDialog *dialog) {
    EntryInfoItem *row = dialog->editableRow(kDate);
    if(!row || !row->valueEditor())
        return nullptr;
    const QList<QAction *> actions = row->valueEditor()->actions();
    if(actions.isEmpty())
        return nullptr;
    actions.first()->trigger();
    return dialog->findChild<DateTimePickerPopup *>();
}

bool writeJpeg(const QString &path) {
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::darkBlue);
    return image.save(path, "JPG");
}

} // namespace

void FileInfoDatePickerTest::theCalendarFillsTheDateFieldAndTypingStillWorks() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString photoPath = root.filePath("gallery/photo.jpg");
    QVERIFY2(writeJpeg(photoPath), "jpeg should be written.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");
    QTRY_COMPARE(grid->itemCount(), 2);
    grid->select(1);

    QVERIFY2(actionManager->invokeAction("toggleImageInfo"),
             "toggleImageInfo action should be invocable.");
    auto *dialog = window->findChild<FileInfoDialog *>();
    QTRY_VERIFY2(dialog != nullptr, "The File info popup should exist.");
    QTRY_VERIFY2(dialog->isVisible(), "The File info popup should be visible.");

    const int exifIndex = tabIndexByText(dialog->tabs(), "EXIF");
    QVERIFY(exifIndex >= 0);
    // A jpeg with no Exif at all still opens the tab: it has empty fields to
    // fill in, and this is the case the picker exists for.
    QTRY_VERIFY2(dialog->tabs()->isTabEnabled(exifIndex),
                 "EXIF tab should be enabled for a writable jpeg.");
    dialog->tabs()->setCurrentIndex(exifIndex);
    QTRY_VERIFY(dialog->editableRow(kDate) != nullptr);
    QVERIFY2(dialog->editableRow(kDate)->valueEditor()->text().isEmpty(),
             "the file has no date yet");

    // --- Picking from the calendar writes the Exif form of the value. ---
    DateTimePickerPopup *picker = openPicker(dialog);
    QVERIFY2(picker != nullptr, "The Date/Time field should offer a calendar drop-down.");
    QVERIFY2(picker->isVisible(), "Triggering the field's calendar action should open it.");
    picker->setDateTime(QDateTime(QDate(2026, 7, 26), QTime(10, 30, 0)));
    auto *setButton = picker->findChild<QPushButton *>(QStringLiteral("dateTimePickerSetButton"));
    QVERIFY(setButton != nullptr);
    setButton->click();
    QVERIFY2(!picker->isVisible(), "Setting a value should close the drop-down.");

    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kDate),
                 QStringLiteral("2026:07:26 10:30:00"));
    QTRY_COMPARE(dialog->editableRow(kDate)->valueEditor()->text(),
                 QStringLiteral("2026:07:26 10:30:00"));

    // Reopening seeds the calendar from the photo's own date rather than today.
    picker = openPicker(dialog);
    QVERIFY(picker != nullptr);
    QCOMPARE(picker->dateTime(), QDateTime(QDate(2026, 7, 26), QTime(10, 30, 0)));
    picker->close();

    // --- Typing still works, unchanged: the field takes a value the picker
    // never produced, and takes an empty one to remove the tag. ---
    QLineEdit *editor = dialog->editableRow(kDate)->valueEditor();
    editor->setText(QStringLiteral("1999:12:31 23:59:59"));
    QTest::keyClick(editor, Qt::Key_Return);
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kDate),
                 QStringLiteral("1999:12:31 23:59:59"));

    editor = dialog->editableRow(kDate)->valueEditor();
    editor->clear();
    QTest::keyClick(editor, Qt::Key_Return);
    QTRY_VERIFY2(!DocumentInfo(photoPath).getEditableTags().contains(kDate),
                 "an emptied field must still remove the tag");

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoDatePickerTest)

#include "test_file_info_date_picker.moc"

// Clicking away from an EXIF field saves it, the same as pressing Enter.
//
// Qt does not do this on its own: most of what surrounds a row - empty tab
// space, a read-only row, a name label - cannot take focus, so a click there
// leaves the field focused and its edit uncommitted. Clicking into a different
// window is the same problem from the other end: focus leaves with
// ActiveWindowFocusReason, which QLineEdit does not treat as finishing an edit.

#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QLineEdit>
#include <QTabWidget>
#include <QTemporaryDir>

#include "components/actionmanager/actionmanager.h"
#include "core.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "gui/dialogs/fileinfodialog.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"
#include "sourcecontainers/documentinfo.h"

class FileInfoClickAwaySavesTest : public QObject {
    Q_OBJECT

private slots:
    void clickingOutsideAFieldSavesItAndDropsFocus();
};

namespace {

const QString kMake  = QStringLiteral("Exif.Image.Make");
const QString kModel = QStringLiteral("Exif.Image.Model");

int tabIndexByText(QTabWidget *tabs, const QString &text) {
    for(int i = 0; i < tabs->count(); ++i) {
        if(tabs->tabText(i) == text)
            return i;
    }
    return -1;
}

// Types into a field the way a user does, leaving it focused and uncommitted.
// hasFocus() is not the test: it also requires an active window, which the
// offscreen platform does not hand out - what matters (and what the dialog
// itself goes by) is the window's focus widget.
void typeInto(QWidget *window, QLineEdit *editor, const QString &text) {
    editor->setFocus(Qt::MouseFocusReason);
    QCOMPARE(window->focusWidget(), editor);
    editor->setText(text);
}

} // namespace

void FileInfoClickAwaySavesTest::clickingOutsideAFieldSavesItAndDropsFocus() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");
    const QString photoPath = root.filePath("gallery/photo.jpg");
    {
        QImage image(32, 24, QImage::Format_RGB32);
        image.fill(Qt::darkYellow);
        QVERIFY2(image.save(photoPath, "JPG"), "jpeg should be written.");
    }

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
    QTRY_VERIFY(dialog->tabs()->isTabEnabled(exifIndex));
    dialog->tabs()->setCurrentIndex(exifIndex);
    QTRY_VERIFY(dialog->editableRow(kMake) != nullptr);

    // --- Clicking dead space inside the tab. Nothing there can take focus, so
    // this is exactly the click that used to lose the edit. ---
    QLineEdit *makeEditor = dialog->editableRow(kMake)->valueEditor();
    typeInto(dialog, makeEditor, QStringLiteral("ClickAwayCam"));
    QWidget *tabPage = dialog->tabs()->widget(exifIndex);
    // Bottom of the tab page, below the last row: empty space, no widget of its
    // own to click.
    QTest::mouseClick(tabPage, Qt::LeftButton, Qt::KeyboardModifiers(),
                      QPoint(tabPage->width() / 2, tabPage->height() - 8));
    QVERIFY2(dialog->focusWidget() != makeEditor,
             "the field should give up focus when clicked away from");
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kMake),
                 QStringLiteral("ClickAwayCam"));

    // --- Clicking a read-only row's label: it consumes the click (its text is
    // selectable) and never moves focus by itself. ---
    QTRY_VERIFY(dialog->editableRow(kModel) != nullptr);
    QLineEdit *modelEditor = dialog->editableRow(kModel)->valueEditor();
    typeInto(dialog, modelEditor, QStringLiteral("Model Z"));
    EntryInfoItem *readOnlyRow = nullptr;
    for(EntryInfoItem *row : dialog->tabs()->widget(exifIndex)->findChildren<EntryInfoItem *>()) {
        if(!row->valueEditor()) {
            readOnlyRow = row;
            break;
        }
    }
    if(readOnlyRow) {
        QTest::mouseClick(readOnlyRow, Qt::LeftButton, Qt::KeyboardModifiers(),
                          QPoint(readOnlyRow->width() / 2, readOnlyRow->height() / 2));
    } else {
        // No read-only row on this file (a jpeg written by Qt carries no other
        // tags); the name label of another editable row is the same kind of
        // unfocusable target.
        QWidget *rowWidget = dialog->editableRow(kMake);
        QTest::mouseClick(rowWidget, Qt::LeftButton, Qt::KeyboardModifiers(), QPoint(8, rowWidget->height() / 2));
    }
    QVERIFY2(dialog->focusWidget() != modelEditor, "clicking another row should end the edit");
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kModel),
                 QStringLiteral("Model Z"));

    // --- Clicking into another window. Focus leaves for a reason Qt does not
    // count as finishing an edit, so the window losing focus has to commit. ---
    QLineEdit *editor = dialog->editableRow(kMake)->valueEditor();
    typeInto(dialog, editor, QStringLiteral("OtherWindowCam"));
    QEvent deactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(dialog, &deactivate);
    QVERIFY2(dialog->focusWidget() != editor, "leaving the window should end the edit");
    QTRY_COMPARE(DocumentInfo(photoPath).getEditableTags().value(kMake),
                 QStringLiteral("OtherWindowCam"));

    if(qEnvironmentVariableIsSet("THUMBGRID_TEST_VISUAL"))
        QTest::qWait(1500);
}

TG_BEHAVIOR_TEST_MAIN(FileInfoClickAwaySavesTest)

#include "test_file_info_click_away_saves.moc"

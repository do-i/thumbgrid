#pragma once

#include <QDialog>
#include <QList>
#include <QMap>
#include <QStringList>

class QTabWidget;
class QVBoxLayout;
class QWidget;
class QLabel;
class QFileInfo;
class QHideEvent;
class QShowEvent;
class QEvent;
class QPushButton;
class QDateTime;
class EntryInfoItem;
class DateTimePickerPopup;

// Non-modal inspector window showing metadata for the current selection.
// Core owns a single instance and retargets it live as the document-view
// image or the grid selection changes (docs/009 §B1). Each metadata tab is
// shown for a format that can carry its own kind of metadata, tags or no tags,
// and hidden entirely for anything else (docs/009 §B2). The gate differs per
// tab because the kinds are not writable in the same containers: EXIF is shown
// where exiv2 can write Exif (jpeg/webp), while XMP and ICC are shown wherever
// the format can hold them (jpeg/png/webp/tiff). A png therefore opens with XMP
// and ICC tabs and no EXIF tab at all.
//
// Every metadata tab now writes, and each writes only its own kind: the EXIF
// tab has its four editable text tags, the XMP tab has a curated typed set plus
// a grid for custom properties, and the ICC tab can discard the profile. Each
// tab carries exactly one removal button, scoped to that tab
// (docs/2026-08-01-001 §6) - so clearing everything is three deliberate acts in
// three places rather than one button that reaches across the file. The
// whole-file strip is deliberately *not* here any more; it remains an
// ActionManager action reachable by shortcut and menu, which is also the only
// route that removes Iptc, since Iptc has no tab of its own.
//
// Buttons follow one visibility rule (docs/2026-08-01-001 §7): hidden when they do
// not belong on the current tab *or* when there is nothing for them to remove,
// and merely disabled when there is something to remove but the file cannot be
// written. Greying therefore means exactly one thing - "there is something
// here, but I cannot write to this file".
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileInfoDialog(QWidget *parent = nullptr);

    // Repopulates the General, EXIF, XMP and ICC tabs for path. An empty or
    // missing path drops to a "No selection" placeholder and hides all three
    // metadata tabs, as does a folder or a non-image. An image keeps whichever
    // of them its format can carry open whether or not it holds anything today
    // - having none is a state worth seeing, and on a writable file worth
    // typing into.
    void setTarget(const QString &path);
    void clearTarget();

    // test access
    QTabWidget *tabs() { return mTabs; }
    QWidget *xmpTab() const { return mXmpTab; }
    QWidget *iccTab() const { return mIccTab; }
    // The read-only property rows on those tabs, in display order.
    QList<EntryInfoItem *> xmpRows() const { return mXmpRows; }
    QList<EntryInfoItem *> iccRows() const { return mIccRows; }
    QLabel *xmpPlaceholder() const { return mXmpPlaceholder; }
    QLabel *iccPlaceholder() const { return mIccPlaceholder; }
    // One scoped removal per metadata tab.
    QPushButton *removeExifButton() { return mRemoveExifButton; }
    QPushButton *removeXmpButton() { return mRemoveXmpButton; }
    QPushButton *removeIccButton() { return mRemoveIccButton; }
    // Reveals the per-row "x" on the custom grid; off by default and reset on
    // every retarget, so a delete mode never follows the user onto another file.
    QPushButton *removeToggleButton() { return mRemoveToggle; }
    // The in-place editor for an editable Exif key, or null when the current
    // target has no editable row for it.
    EntryInfoItem *editableRow(const QString &key) const { return mEditableRows.value(key, nullptr); }
    EntryInfoItem *editableXmpRow(const QString &key) const { return mXmpEditableRows.value(key, nullptr); }
    // Custom-property grid rows, in display order. The last one is always the
    // blank affordance row.
    QList<EntryInfoItem *> customXmpRows() const { return mCustomXmpRows; }
    // Explains why a tab is read-only, or that a list was capped.
    QLabel *xmpNotice() const { return mXmpNotice; }

signals:
    // One signal per committed field. The EXIF tab has no Save button, so each
    // row is its own write. The path is carried rather than re-derived because
    // this window targets one file while Core::selectedPath() takes the last of
    // a multi-selection.
    void metadataEditRequested(const QString &path, const QString &key, const QString &value);
    // A curated XMP key. QStringList rather than QString because Seq and Bag
    // values are lists, and the displayed joined form cannot be re-split.
    void xmpEditRequested(const QString &path, const QString &key, const QStringList &values);
    // One custom grid row committed. oldKey is empty when the row is new; when
    // it differs from newKey the row was renamed, which in exiv2 is an erase
    // plus an add rather than a rename. namespaceUri is filled in only when the
    // prefix is one the file does not already declare.
    void customXmpEditRequested(const QString &path, const QString &oldKey,
                                const QString &newKey, const QString &value,
                                const QString &namespaceUri);
    void customXmpRemoveRequested(const QString &path, const QString &key);
    // Scoped removals, one per tab. Core raises the confirmation.
    void removeAllExifRequested(const QString &path);
    void removeAllXmpRequested(const QString &path);
    void removeIccProfileRequested(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    // Sees clicks that land anywhere in the application while this window is
    // open; see the comment in showEvent().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void populateGeneralTab(const QString &path);
    void populateExifTab(const QString &path);
    void populateXmpTab(const QString &path);
    void populateIccTab(const QString &path);
    void clearGeneralRows();
    void addGeneralRow(const QString &name, const QString &value);
    void clearExifRows();
    void addExifRow(const QString &name, const QString &value);
    void clearXmpRows();
    void addXmpRow(const QString &name, const QString &value);
    void clearIccRows();
    void addIccRow(const QString &name, const QString &value);
    // An editable tag: same row shape, but the value is an input field wired
    // back to commitEditableTag().
    void addEditableExifRow(const QString &key, const QString &value);
    // A curated XMP key, with the widget its declared type calls for: a plain
    // field for Text and LangAlt, a short one-item-per-line editor for Seq and
    // Bag.
    void addEditableXmpRow(const QString &key, const QStringList &values);
    // One custom-property grid row. An empty key and value make the trailing
    // affordance row, which is never written.
    EntryInfoItem *addCustomXmpRow(const QString &key, const QString &value, bool editable);
    // Keeps exactly one blank row at the end of the grid: filling the last one
    // appends a fresh blank beneath it.
    void ensureTrailingBlankRow();
    void commitCustomXmpRow(EntryInfoItem *row);
    // A section title inside a rows container ("Standard properties").
    void addXmpSectionHeader(const QString &title);
    // Hangs a calendar drop-down off the Date/Time field, without taking away
    // the ability to type (or blank) it.
    void addDateTimePickerAction(EntryInfoItem *row);
    void onDateTimePicked(const QDateTime &dateTime);
    // Validates, then asks Core to write one tag. Rejected input never leaves
    // the dialog.
    void commitEditableTag(const QString &key, const QString &value);
    void commitEditableXmpTag(const QString &key, const QString &text);
    void showExifError(const QString &message);
    void showXmpNotice(const QString &message);
    // Ends an in-progress row edit when the user clicks away from the field
    // (clicked == nullptr when the whole window is losing focus).
    void commitFocusedEditor(QWidget *clicked);
    // Applies the one visibility rule to all three removal buttons.
    void updateActionButtons();

    // The "Symlink to" row's value: the target path, tagged as broken when the
    // link resolves to nothing.
    static QString symlinkTargetString(const QFileInfo &fi);
    static QString permissionsString(const QString &path);

    QTabWidget *mTabs = nullptr;
    QWidget *mGeneralTab = nullptr;
    QWidget *mExifTab = nullptr;
    QLabel *mPlaceholder = nullptr;
    QWidget *mRowsContainer = nullptr;
    QVBoxLayout *mRowsLayout = nullptr;
    QList<EntryInfoItem *> mRows;
    QWidget *mExifRowsContainer = nullptr;
    QVBoxLayout *mExifRowsLayout = nullptr;
    QList<EntryInfoItem *> mExifRows;
    // Editable rows, by exiv2 key, plus the on-disk values they started from -
    // the fallback a rejected edit is reverted to.
    QMap<QString, EntryInfoItem *> mEditableRows;
    QMap<QString, QString> mEditableOriginals;
    // Shown in place of the rows when an image simply has no tags.
    QLabel *mExifPlaceholder = nullptr;
    QLabel *mExifError = nullptr;

    QWidget *mXmpTab = nullptr;
    QWidget *mXmpRowsContainer = nullptr;
    QVBoxLayout *mXmpRowsLayout = nullptr;
    QList<EntryInfoItem *> mXmpRows;
    QList<QLabel *> mXmpHeaders;
    QMap<QString, EntryInfoItem *> mXmpEditableRows;
    QList<EntryInfoItem *> mCustomXmpRows;
    // The key each grid row was built from, so a renamed row can erase the one
    // it replaces. Empty for the trailing blank row.
    QMap<EntryInfoItem *, QString> mCustomXmpOriginalKeys;
    QLabel *mXmpPlaceholder = nullptr;
    QLabel *mXmpNotice = nullptr;

    QWidget *mIccTab = nullptr;
    QWidget *mIccRowsContainer = nullptr;
    QVBoxLayout *mIccRowsLayout = nullptr;
    QList<EntryInfoItem *> mIccRows;
    QLabel *mIccPlaceholder = nullptr;

    // Built on first use and kept: it outlives the rows, which are rebuilt on
    // every retarget.
    DateTimePickerPopup *mDateTimePicker = nullptr;
    QPushButton *mRemoveExifButton = nullptr;
    QPushButton *mRemoveXmpButton = nullptr;
    QPushButton *mRemoveIccButton = nullptr;
    QPushButton *mRemoveToggle = nullptr;
    // What updateActionButtons() needs to know about the current target, worked
    // out once per populate rather than by reopening the file per button.
    bool mHasExif = false;
    bool mHasXmp = false;
    bool mHasIcc = false;
    bool mExifWritable = false;
    bool mXmpWritable = false;
    bool mIccWritable = false;
    QString mTargetPath;
};

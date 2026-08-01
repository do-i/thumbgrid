#pragma once

#include <QDialog>
#include <QList>
#include <QMap>

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
// tab because the tabs differ in purpose: EXIF is shown only where exiv2 can
// *write* Exif (jpeg/webp), since a png has nowhere to store an edit, while
// XMP and ICC only read and so are shown wherever the format can *hold* them
// (jpeg/png/webp/tiff). A png therefore opens with XMP and ICC tabs and no
// EXIF tab at all.
//
// The tabs differ in kind, not just in content: General, XMP and ICC are
// read-only views, so they carry no buttons at all, while EXIF is where the
// file is written - the editable tags are in-place input fields and the Clear
// metadata button rides along with them. XMP and ICC stay read-only because
// there is nothing here to type into: an XMP property is a typed value
// (LangAlt, Seq, Bag) rather than a line of text, and "editing" an ICC profile
// means replacing a binary blob. Anything that acts on the file therefore
// appears only while the EXIF tab is current.
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileInfoDialog(QWidget *parent = nullptr);

    // Repopulates the General, EXIF, XMP and ICC tabs for path. An empty or
    // missing path drops to a "No selection" placeholder and hides all three
    // metadata tabs, as does a folder or a non-image. An image keeps whichever
    // of them its format can carry open whether or not it holds anything today
    // - having none is a state worth seeing (and, on EXIF, when the file is
    // also writable, worth typing into).
    void setTarget(const QString &path);
    void clearTarget();

    // test access
    QTabWidget *tabs() { return mTabs; }
    // The read-only metadata tabs, for asserting visibility: they are hidden
    // rather than removed, so tabs()->indexOf() still finds them.
    QWidget *xmpTab() const { return mXmpTab; }
    QWidget *iccTab() const { return mIccTab; }
    // The rows currently listed on those tabs, in display order (ICC keeps the
    // order DocumentInfo::getIccProfileInfo() hands back).
    QList<EntryInfoItem *> xmpRows() const { return mXmpRows; }
    QList<EntryInfoItem *> iccRows() const { return mIccRows; }
    // Shown in place of the rows when the format can carry the metadata but
    // this file holds none; visible only while its tab is.
    QLabel *xmpPlaceholder() const { return mXmpPlaceholder; }
    QLabel *iccPlaceholder() const { return mIccPlaceholder; }
    // Named for the objectName/action it drives (stripMetadata); the button
    // itself reads "Clear metadata".
    QPushButton *stripMetadataButton() { return mStripButton; }
    // The in-place editor for an editable Exif key, or null when the current
    // target has no editable row for it.
    EntryInfoItem *editableRow(const QString &key) const { return mEditableRows.value(key, nullptr); }

signals:
    // Same reason as stripMetadataRequested: this window targets one file, so it
    // names it rather than letting Core re-derive it from the selection. One
    // signal per committed field - the EXIF tab has no Save button, so each row
    // is its own write.
    void metadataEditRequested(const QString &path, const QString &key, const QString &value);
    // Carries the path rather than letting Core re-derive it: this window shows
    // exactly one file, while Core::selectedPath() takes the *last* of a
    // multi-selection, so re-deriving could strip a different file than the one
    // whose EXIF rows the user is looking at.
    void stripMetadataRequested(const QString &path);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    // Sees clicks that land anywhere in the application while this window is
    // open; see the comment in showEvent().
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void populateGeneralTab(const QString &path);
    void populateExifTab(const QString &path);
    // Same rule shape as populateExifTab(), read-only: shown when the format
    // can carry the metadata, with a placeholder when this file carries none.
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
    // Hangs a calendar drop-down off the Date/Time field, without taking away
    // the ability to type (or blank) it.
    void addDateTimePickerAction(EntryInfoItem *row);
    void onDateTimePicked(const QDateTime &dateTime);
    // Validates, then asks Core to write one tag. Rejected input never leaves
    // the dialog.
    void commitEditableTag(const QString &key, const QString &value);
    void showExifError(const QString &message);
    // Ends an in-progress row edit when the user clicks away from the field
    // (clicked == nullptr when the whole window is losing focus).
    void commitFocusedEditor(QWidget *clicked);
    // Enabled only for a still or animated image that exists on disk - the only
    // thing DocumentInfo::stripMetadata() can actually write.
    void updateStripButton(const QString &path);
    // Keeps the button row out of the read-only tabs (General, XMP, ICC).
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
    // The two read-only metadata tabs. Same shape as the EXIF tab minus the
    // editable rows and the error label - neither writes, so neither can fail
    // a write.
    QWidget *mXmpTab = nullptr;
    QWidget *mXmpRowsContainer = nullptr;
    QVBoxLayout *mXmpRowsLayout = nullptr;
    QList<EntryInfoItem *> mXmpRows;
    QLabel *mXmpPlaceholder = nullptr;
    QWidget *mIccTab = nullptr;
    QWidget *mIccRowsContainer = nullptr;
    QVBoxLayout *mIccRowsLayout = nullptr;
    QList<EntryInfoItem *> mIccRows;
    QLabel *mIccPlaceholder = nullptr;
    // Built on first use and kept: it outlives the rows, which are rebuilt on
    // every retarget.
    DateTimePickerPopup *mDateTimePicker = nullptr;
    QPushButton *mStripButton = nullptr;
    QString mTargetPath;
};

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
class QCheckBox;
class QScrollArea;
class QDateTime;
class EntryInfoItem;
class DateTimePickerPopup;

// Non-modal metadata inspector; Core owns one instance and retargets it as the selection changes.
// Each metadata tab shows for formats that can carry its kind, tags or not.
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileInfoDialog(QWidget *parent = nullptr);

    // Empty/missing path, folders and non-images show "No selection" and hide the metadata tabs.
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
    // One signal per committed field (no Save button). Carries the path: Core::selectedPath() is the last of a multi-selection.
    void metadataEditRequested(const QString &path, const QString &key, const QString &value);
    // A curated XMP key. QStringList rather than QString because Seq and Bag
    // values are lists, and the displayed joined form cannot be re-split.
    void xmpEditRequested(const QString &path, const QString &key, const QStringList &values);
    // oldKey empty for a new row; differs from newKey on rename (erase + add). namespaceUri only for undeclared prefixes.
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
    // plain field for Text/LangAlt, one-item-per-line editor for Seq/Bag
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
    QScrollArea *mExifScroll = nullptr;
    QCheckBox *mShowAllTagsCheckBox = nullptr;

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

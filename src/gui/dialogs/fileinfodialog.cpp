#include "fileinfodialog.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gui/customwidgets/datetimepickerpopup.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "settings.h"
#include "sourcecontainers/documentinfo.h"

namespace {

// The one Exif tag with a machine-readable format, and so the one the picker
// can offer to fill in.
const QLatin1String kDateTimeKey("Exif.Image.DateTime");
// Exif 2.3 §4.6.4. Also the display format, so what the picker writes and what
// the user types are the same string.
const QLatin1String kExifDateTimeFormat("yyyy:MM:dd HH:mm:ss");

QString formatDateTime(const QDateTime &dt) {
    if(!dt.isValid())
        return QStringLiteral("—"); // em dash: unavailable on this filesystem
    return QLocale().toString(dt, QLocale::ShortFormat);
}

// Drawn rather than shipped as an asset: it is one 16px glyph that has to match
// the row text in whatever theme is active, and drawing it costs less than a
// pair of @1x/@2x pixmaps plus a recolor pass.
QIcon calendarIcon(const QColor &color) {
    const int size = 16;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing, false);
    QPen pen(color);
    pen.setWidth(1);
    p.setPen(pen);
    const QRect body(1, 3, 13, 11);
    p.drawRect(body);
    // Header band and the two hanging rings, the parts that make it read as a
    // calendar rather than a plain box at this size.
    p.fillRect(QRect(body.left() + 1, body.top() + 1, body.width() - 1, 3), color);
    p.drawLine(4, 1, 4, 3);
    p.drawLine(11, 1, 11, 3);
    // Two rows of "days".
    for(int row = 0; row < 2; ++row) {
        for(int col = 0; col < 3; ++col)
            p.fillRect(QRect(3 + col * 4, 8 + row * 3, 2, 2), color);
    }
    return QIcon(pixmap);
}

} // namespace

FileInfoDialog::FileInfoDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("File info"));
    setModal(false);
    resize(440, 440);

    auto *layout = new QVBoxLayout(this);
    mTabs = new QTabWidget(this);
    layout->addWidget(mTabs);

    // General tab: a placeholder shown when nothing is targeted, plus a rows
    // container that setTarget() rebuilds from EntryInfoItem widgets.
    auto *generalTab = new QWidget(mTabs);
    auto *generalLayout = new QVBoxLayout(generalTab);
    mPlaceholder = new QLabel(tr("No selection"), generalTab);
    mPlaceholder->setAlignment(Qt::AlignCenter);
    generalLayout->addWidget(mPlaceholder);
    mRowsContainer = new QWidget(generalTab);
    mRowsLayout = new QVBoxLayout(mRowsContainer);
    mRowsLayout->setContentsMargins(0, 0, 0, 0);
    mRowsLayout->setSpacing(0);
    generalLayout->addWidget(mRowsContainer);
    generalLayout->addStretch(1);
    mGeneralTab = generalTab;
    mTabs->addTab(generalTab, tr("General"));

    // EXIF tab: rows are (re)built by populateExifTab(); the tab is shown for
    // a format Exiv2 can write Exif into (jpeg/webp), with a placeholder for
    // the ones that currently carry none, and hidden entirely for kinds that
    // can't store Exif at all (png, gif, video, text, folders).
    mExifTab = new QWidget(mTabs);
    auto *exifLayout = new QVBoxLayout(mExifTab);
    mExifRowsContainer = new QWidget(mExifTab);
    mExifRowsLayout = new QVBoxLayout(mExifRowsContainer);
    mExifRowsLayout->setContentsMargins(0, 0, 0, 0);
    mExifRowsLayout->setSpacing(0);
    exifLayout->addWidget(mExifRowsContainer);
    mExifPlaceholder = new QLabel(tr("No metadata"), mExifTab);
    mExifPlaceholder->setAlignment(Qt::AlignCenter);
    mExifPlaceholder->hide();
    exifLayout->addWidget(mExifPlaceholder);
    // Sits under the rows, where the field that was rejected is: the tab has no
    // Save button to attach a message to.
    mExifError = new QLabel(mExifTab);
    mExifError->setObjectName(QStringLiteral("metadataEditError"));
    mExifError->setWordWrap(true);
    mExifError->hide();
    exifLayout->addWidget(mExifError);
    exifLayout->addStretch(1);
    mTabs->addTab(mExifTab, tr("EXIF"));
    mTabs->setTabVisible(mTabs->indexOf(mExifTab), false);

    // XMP tab: the EXIF tab's shape without the editable rows or the error
    // label - it only reads, so it has no write to report on. Shown for any
    // format that can hold an XMP packet (jpeg/png/webp/tiff), which is wider
    // than EXIF's "exiv2 can write here", so a png gets this tab and no EXIF
    // one.
    mXmpTab = new QWidget(mTabs);
    auto *xmpLayout = new QVBoxLayout(mXmpTab);
    mXmpRowsContainer = new QWidget(mXmpTab);
    mXmpRowsLayout = new QVBoxLayout(mXmpRowsContainer);
    mXmpRowsLayout->setContentsMargins(0, 0, 0, 0);
    mXmpRowsLayout->setSpacing(0);
    xmpLayout->addWidget(mXmpRowsContainer);
    mXmpPlaceholder = new QLabel(tr("No XMP metadata"), mXmpTab);
    mXmpPlaceholder->setAlignment(Qt::AlignCenter);
    mXmpPlaceholder->hide();
    xmpLayout->addWidget(mXmpPlaceholder);
    xmpLayout->addStretch(1);
    mTabs->addTab(mXmpTab, tr("XMP"));
    mTabs->setTabVisible(mTabs->indexOf(mXmpTab), false);

    // ICC tab: same again, listing a derived summary of the embedded colour
    // profile rather than the blob itself.
    mIccTab = new QWidget(mTabs);
    auto *iccLayout = new QVBoxLayout(mIccTab);
    mIccRowsContainer = new QWidget(mIccTab);
    mIccRowsLayout = new QVBoxLayout(mIccRowsContainer);
    mIccRowsLayout->setContentsMargins(0, 0, 0, 0);
    mIccRowsLayout->setSpacing(0);
    iccLayout->addWidget(mIccRowsContainer);
    mIccPlaceholder = new QLabel(tr("No ICC profile"), mIccTab);
    mIccPlaceholder->setAlignment(Qt::AlignCenter);
    mIccPlaceholder->hide();
    iccLayout->addWidget(mIccPlaceholder);
    iccLayout->addStretch(1);
    mTabs->addTab(mIccTab, tr("ICC"));
    mTabs->setTabVisible(mTabs->indexOf(mIccTab), false);

    // Shown only while the EXIF tab is current (updateActionButtons): General,
    // XMP and ICC are read-only views, so they offer no action at all. Red
    // (#stripMetadataButton, styled with the same danger tokens as the delete
    // confirmations) because it
    // rewrites the file on disk and cannot be undone. Core raises the
    // confirmation - see Core::stripMetadataAt().
    mStripButton = new QPushButton(tr("Clear metadata"), this);
    mStripButton->setObjectName(QStringLiteral("stripMetadataButton"));
    mStripButton->setToolTip(tr("Permanently remove all Exif, IPTC and XMP metadata from this file"));
    mStripButton->setCursor(Qt::PointingHandCursor);
    // Never the dialog's default button: Enter is for dismissing an inspector
    // window, not for destroying data.
    mStripButton->setAutoDefault(false);
    mStripButton->setDefault(false);
    connect(mStripButton, &QPushButton::clicked, this, [this]() {
        if(!mTargetPath.isEmpty())
            emit stripMetadataRequested(mTargetPath);
    });

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->addStretch(1);
    buttonRow->addWidget(mStripButton);
    layout->addLayout(buttonRow);

    connect(mTabs, &QTabWidget::currentChanged, this, [this]() { updateActionButtons(); });

    const QByteArray geometry = settings->fileInfoDialogGeometry();
    if(!geometry.isEmpty())
        restoreGeometry(geometry);

    clearTarget();
}

void FileInfoDialog::showEvent(QShowEvent *event) {
    // Watching the application, not just this window: a click "outside the
    // field" mostly lands on something that cannot take focus (empty tab space,
    // a read-only row, a name label), and Qt leaves focus - and therefore the
    // pending edit - exactly where it was. Those clicks never reach this dialog
    // either, because the widget under the cursor consumes or ignores them
    // without ever routing them here. An application filter is the one place
    // that sees them all. Installed only while the window is up.
    qApp->installEventFilter(this);
    QDialog::showEvent(event);
}

void FileInfoDialog::hideEvent(QHideEvent *event) {
    qApp->removeEventFilter(this);
    // Dismissed via Core::toggleFileInfoDialog()'s hide() far more often than
    // via the window's close button, so geometry is saved here rather than
    // in closeEvent (which a plain hide() never triggers).
    settings->setFileInfoDialogGeometry(saveGeometry());
    QDialog::hideEvent(event);
}

bool FileInfoDialog::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QEvent::MouseButtonPress)
        commitFocusedEditor(qobject_cast<QWidget *>(watched));
    // Clicking straight into another window takes the focus away with
    // ActiveWindowFocusReason, which QLineEdit deliberately does not treat as
    // finishing an edit - so the mouse press above arrives too late to find the
    // field still focused. Deactivation is the moment to commit instead.
    else if(watched == this && event->type() == QEvent::WindowDeactivate)
        commitFocusedEditor(nullptr);
    return QDialog::eventFilter(watched, event);
}

// Ends the edit explicitly rather than by dropping focus and waiting for Qt's
// editingFinished: on window deactivation Qt has already taken focus away
// (ActiveWindowFocusReason) by the time this runs, so clearing it again reports
// nothing. The commit is idempotent, so the focus-out that follows is a no-op.
void FileInfoDialog::commitFocusedEditor(QWidget *clicked) {
    QWidget *focused = focusWidget();
    if(!focused || !isAncestorOf(focused))
        return;
    // A click inside the field being edited is a cursor move, not a commit.
    if(clicked && (clicked == focused || focused->isAncestorOf(clicked)))
        return;
    EntryInfoItem *editing = nullptr;
    for(EntryInfoItem *row : std::as_const(mEditableRows)) {
        if(row->valueEditor() == focused) {
            editing = row;
            break;
        }
    }
    // Focus sitting anywhere else in this window (the Clear metadata button, a
    // tab) is not an edit in progress and is left alone.
    if(!editing)
        return;
    editing->commitEdit();
    focused->clearFocus();
}

void FileInfoDialog::setTarget(const QString &path) {
    mTargetPath = path;
    populateGeneralTab(path);
    populateExifTab(path);
    populateXmpTab(path);
    populateIccTab(path);
    updateStripButton(path);
    updateActionButtons();
}

// General, XMP and ICC show nothing that can be changed from here, so they carry
// no buttons; everything that writes the file belongs with the EXIF fields it
// writes. Keyed on the EXIF tab specifically rather than on "not General", so
// adding read-only tabs never leaks a write action onto one. Visibility, not
// enablement: on a read-only tab there is nothing to explain by showing a
// greyed-out button.
void FileInfoDialog::updateActionButtons() {
    mStripButton->setVisible(mTabs->currentIndex() == mTabs->indexOf(mExifTab));
}

// Same type test populateExifTab() uses, and the same reason: DocumentInfo is
// cheap enough for an ad hoc query on a path. A folder, a video, or a missing
// file leaves the button disabled rather than hidden, so its absence never
// reads as "this file has no metadata".
void FileInfoDialog::updateStripButton(const QString &path) {
    bool strippable = false;
    QFileInfo fi(path);
    if(!path.isEmpty() && fi.isFile() && fi.isWritable()) {
        DocumentInfo docInfo(path);
        strippable = (docInfo.type() == DocumentType::STATIC ||
                      docInfo.type() == DocumentType::ANIMATED);
    }
    mStripButton->setEnabled(strippable);
}

void FileInfoDialog::clearTarget() {
    setTarget(QString());
}

void FileInfoDialog::clearGeneralRows() {
    for(EntryInfoItem *row : mRows)
        delete row;
    mRows.clear();
}

void FileInfoDialog::addGeneralRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mRowsContainer);
    row->setInfo(name, value);
    mRowsLayout->addWidget(row);
    mRows.append(row);
}

void FileInfoDialog::populateGeneralTab(const QString &path) {
    clearGeneralRows();

    QFileInfo fi(path);
    const bool isLink = fi.isSymLink();
    // exists() resolves the link, so a dangling symlink reads as absent - but it
    // is still a real entry the user selected and asked about, so it gets its own
    // rows instead of the "No selection" placeholder.
    if(path.isEmpty() || (!fi.exists() && !isLink)) {
        mRowsContainer->hide();
        mPlaceholder->show();
        return;
    }
    mPlaceholder->hide();
    mRowsContainer->show();

    // Path first for both files and folders, and it is deliberately *not*
    // canonicalized: the entry the user picked is the link itself. Everything
    // below is read through it, which is what the Symlink to row announces.
    addGeneralRow(tr("Path"), fi.absoluteFilePath());
    if(isLink)
        addGeneralRow(tr("Symlink to"), symlinkTargetString(fi));

    // A broken link has no target to read size, permissions or timestamps from;
    // QFileInfo would answer 0 bytes, "---------" and "—" for all of them, which
    // reads as "an empty file" rather than "there is nothing at the other end".
    if(isLink && !fi.exists())
        return;

    if(fi.isDir()) {
        addGeneralRow(tr("Permissions"), permissionsString(path));
        if(!fi.owner().isEmpty())
            addGeneralRow(tr("Owner"), fi.owner());
        // Direct children only, counted in one non-recursive scan (hidden
        // entries excluded, matching the file managers users compare against).
        int files = 0, folders = 0;
        const QFileInfoList entries =
            QDir(path).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for(const QFileInfo &entry : entries)
            entry.isDir() ? ++folders : ++files;
        addGeneralRow(tr("Contains"), QStringLiteral("%1, %2")
                                          .arg(tr("%n file(s)", "", files),
                                               tr("%n folder(s)", "", folders)));
        addGeneralRow(tr("Modified"), formatDateTime(fi.lastModified()));
        return;
    }

    // Folder entry sizes are meaningless to users, so the size row is a file-
    // only row; folders show "Contains" instead.
    addGeneralRow(tr("Size"), QLocale().formattedDataSize(fi.size()));
    addGeneralRow(tr("Permissions"), permissionsString(path));
    if(!fi.owner().isEmpty())
        addGeneralRow(tr("Owner"), fi.owner());
    addGeneralRow(tr("Created"), formatDateTime(fi.birthTime()));
    addGeneralRow(tr("Modified"), formatDateTime(fi.lastModified()));
}

void FileInfoDialog::populateExifTab(const QString &path) {
    clearExifRows();
    mExifError->hide();
    const int exifIndex = mTabs->indexOf(mExifTab);
    const bool wasCurrent = (mTabs->currentIndex() == exifIndex);

    // Constructed locally rather than routed in from Core: DocumentInfo is
    // designed for cheap ad hoc queries on a path (Core itself does this,
    // e.g. Core::isSupportedImage in core.cpp), so there is no heavyweight
    // shared state to reuse here.
    QMap<QString, QString> tags;
    QFileInfo fi(path);
    bool editable = false;
    // Whether the format is one exiv2 can write Exif into at all (jpeg/webp),
    // which is what decides the tab - not whether this particular file is
    // currently writable, nor whether it happens to carry any tags today. A
    // photo with its metadata already stripped is exactly the case where
    // someone opens this tab to put a date or a comment back; a png has
    // nowhere for that write to go, so it gets no tab to open.
    bool canCarryExif = false;
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsMetadataEditing(path)) {
            canCarryExif = true;
            // Honor the global metadata verbosity toggle the same way the
            // document view does (Core::showDocument).
            tags = settings->showFullMetadata() ? docInfo.getAllTags()
                                                : docInfo.getExifTags();
            // The format supports writing; whether we are allowed to write to
            // *this* file also needs it to be writable. A read-only jpeg still
            // lists its tags, just as text. The fields come first: they are
            // the only rows here the user can act on, and the read-only dump
            // below them can run to dozens of entries.
            editable = fi.isWritable();
            if(editable) {
                mEditableOriginals = docInfo.getEditableTags();
                for(const QString &key : DocumentInfo::editableTagKeys())
                    addEditableExifRow(key, mEditableOriginals.value(key));
            }
        }
    }

    // The read-only dump keys its rows differently per verbosity mode -
    // getAllTags() by exiv2 key ("Exif.Image.Make"), getExifTags() by the
    // display label QObject::tr("Make") - and both forms of the editable four
    // are dropped, so each of those tags appears exactly once: as its field.
    // The labels are looked up in QObject's context because that is the context
    // DocumentInfo::loadExifTags() translated them in.
    const QStringList compactLabels = {QObject::tr("Make"), QObject::tr("Model"),
                                       QObject::tr("Date/Time"), QObject::tr("UserComment")};
    for(auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
        if(editable && (DocumentInfo::editableTagKeys().contains(it.key()) ||
                        compactLabels.contains(it.key())))
            continue;
        addExifRow(it.key(), it.value());
    }

    mTabs->setTabVisible(exifIndex, canCarryExif);
    // An image with nothing to list says so, rather than showing a blank pane:
    // "no metadata" and "the tab failed to fill in" look identical otherwise.
    // Folders, videos and text files have no such tab to land on at all, so it
    // is hidden rather than merely disabled, and whatever tab they arrive with
    // has to fall back to General.
    mExifPlaceholder->setVisible(canCarryExif && mExifRows.isEmpty());
    if(!canCarryExif && wasCurrent)
        mTabs->setCurrentIndex(mTabs->indexOf(mGeneralTab));
}

void FileInfoDialog::clearExifRows() {
    for(EntryInfoItem *row : mExifRows)
        delete row;
    mExifRows.clear();
    mEditableRows.clear();
    mEditableOriginals.clear();
}

void FileInfoDialog::addExifRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mExifRowsContainer);
    row->setInfo(name, value);
    mExifRowsLayout->addWidget(row);
    mExifRows.append(row);
}

void FileInfoDialog::addEditableExifRow(const QString &key, const QString &value) {
    auto *row = new EntryInfoItem(mExifRowsContainer);
    row->setInfo(DocumentInfo::editableTagLabel(key), value);
    row->setEditable(true);
    if(key == kDateTimeKey) {
        row->setValuePlaceholder(QStringLiteral("YYYY:MM:DD HH:MM:SS"));
        addDateTimePickerAction(row);
    }
    connect(row, &EntryInfoItem::valueEdited, this,
            [this, key](const QString &newValue) { commitEditableTag(key, newValue); });
    mExifRowsLayout->addWidget(row);
    mExifRows.append(row);
    mEditableRows.insert(key, row);
}

// The picker is an addition to the field, not a replacement for it: the row
// still takes typed text (and still takes an empty one, which is how the tag
// gets removed - something no date widget can express). It only offers the
// calendar to whoever would rather not remember Exif's colon-separated form.
void FileInfoDialog::addDateTimePickerAction(EntryInfoItem *row) {
    QLineEdit *editor = row->valueEditor();
    if(!editor)
        return;
    auto *action = editor->addAction(calendarIcon(settings->colorScheme().text_hc2),
                                     QLineEdit::TrailingPosition);
    action->setObjectName(QStringLiteral("dateTimePickerAction"));
    action->setToolTip(tr("Pick a date and time"));
    connect(action, &QAction::triggered, this, [this, row, editor]() {
        if(!mDateTimePicker) {
            mDateTimePicker = new DateTimePickerPopup(this);
            connect(mDateTimePicker, &DateTimePickerPopup::dateTimePicked, this,
                    &FileInfoDialog::onDateTimePicked);
        }
        // Opens on what the field already holds, so the picker starts from the
        // photo's own date rather than from today whenever there is one.
        mDateTimePicker->setDateTime(
            QDateTime::fromString(editor->text(), kExifDateTimeFormat));
        mDateTimePicker->popupUnder(editor);
    });
}

void FileInfoDialog::onDateTimePicked(const QDateTime &dateTime) {
    EntryInfoItem *row = mEditableRows.value(kDateTimeKey);
    if(!row || mTargetPath.isEmpty())
        return;
    const QString text = dateTime.toString(kExifDateTimeFormat);
    // setInfo() rather than only setting the editor text: it moves the row's
    // own idea of the value too, so the focus-out that follows the popup
    // closing does not report the same pick a second time.
    row->setInfo(DocumentInfo::editableTagLabel(kDateTimeKey), text);
    commitEditableTag(kDateTimeKey, text);
}

// Nothing here writes the file - Core does, so the confirmation, reload and
// error reporting for metadata stay in one place (Core::saveMetadataTagAt).
void FileInfoDialog::commitEditableTag(const QString &key, const QString &value) {
    if(mTargetPath.isEmpty())
        return;
    // Exif.Image.DateTime has exactly one legal form. Caught here rather than in
    // the write so the message can name the format; the field goes back to what
    // is on disk, because a row of an inspector window showing something the
    // file does not contain is worse than losing a mistyped date.
    if(key == kDateTimeKey && !value.isEmpty() &&
       !DocumentInfo::isValidExifDateTime(value)) {
        if(EntryInfoItem *row = mEditableRows.value(key))
            row->setInfo(DocumentInfo::editableTagLabel(key), mEditableOriginals.value(key));
        showExifError(tr("Date/Time must look like 2026:07:26 10:30:00."));
        return;
    }
    mExifError->hide();
    emit metadataEditRequested(mTargetPath, key, value);
}

void FileInfoDialog::showExifError(const QString &message) {
    mExifError->setText(message);
    mExifError->show();
}

// Same rule shape as populateExifTab() - existing file, still or animated image,
// format that can carry the metadata - with a wider predicate, because this tab
// only reads: supportsXmp() asks whether the container can hold an XMP packet,
// not whether exiv2 would rewrite Exif into it. A png passes here and fails
// there, which is why it shows this tab and no EXIF one.
void FileInfoDialog::populateXmpTab(const QString &path) {
    clearXmpRows();
    const int xmpIndex = mTabs->indexOf(mXmpTab);
    const bool wasCurrent = (mTabs->currentIndex() == xmpIndex);

    QMap<QString, QString> tags;
    QFileInfo fi(path);
    bool canCarryXmp = false;
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsXmp(path)) {
            canCarryXmp = true;
            tags = docInfo.getXmpTags();
        }
    }

    // Keyed by exiv2 key ("Xmp.dc.title"), listed in the map's own order: XMP
    // has no display-label form to sort by, and grouping by schema prefix is
    // what sorting the keys gives anyway.
    for(auto it = tags.constBegin(); it != tags.constEnd(); ++it)
        addXmpRow(it.key(), it.value());

    mTabs->setTabVisible(xmpIndex, canCarryXmp);
    // A format that can carry XMP but currently carries none says so rather than
    // showing a blank pane, exactly as the EXIF tab does.
    mXmpPlaceholder->setVisible(canCarryXmp && mXmpRows.isEmpty());
    if(!canCarryXmp && wasCurrent)
        mTabs->setCurrentIndex(mTabs->indexOf(mGeneralTab));
}

// As populateXmpTab(), for the embedded colour profile.
void FileInfoDialog::populateIccTab(const QString &path) {
    clearIccRows();
    const int iccIndex = mTabs->indexOf(mIccTab);
    const bool wasCurrent = (mTabs->currentIndex() == iccIndex);

    // A list rather than a map, and iterated in the order DocumentInfo built it:
    // the four rows read as a description ("what is it" through "how big is
    // it"), which sorting by key would scramble.
    QList<QPair<QString, QString>> info;
    QFileInfo fi(path);
    bool canCarryIcc = false;
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsIccProfile(path)) {
            canCarryIcc = true;
            info = docInfo.getIccProfileInfo();
        }
    }

    for(const auto &row : std::as_const(info))
        addIccRow(row.first, row.second);

    mTabs->setTabVisible(iccIndex, canCarryIcc);
    mIccPlaceholder->setVisible(canCarryIcc && mIccRows.isEmpty());
    if(!canCarryIcc && wasCurrent)
        mTabs->setCurrentIndex(mTabs->indexOf(mGeneralTab));
}

void FileInfoDialog::clearXmpRows() {
    for(EntryInfoItem *row : mXmpRows)
        delete row;
    mXmpRows.clear();
}

void FileInfoDialog::addXmpRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mXmpRowsContainer);
    row->setInfo(name, value);
    mXmpRowsLayout->addWidget(row);
    mXmpRows.append(row);
}

void FileInfoDialog::clearIccRows() {
    for(EntryInfoItem *row : mIccRows)
        delete row;
    mIccRows.clear();
}

void FileInfoDialog::addIccRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mIccRowsContainer);
    row->setInfo(name, value);
    mIccRowsLayout->addWidget(row);
    mIccRows.append(row);
}

// symLinkTarget() is a string, not a promise: it is filled in for a dangling
// link too, which is exactly the case worth naming - a link whose target was
// moved or deleted is indistinguishable from a working one until the target is
// spelled out. fi.exists() (which resolves the whole chain) decides broken,
// rather than testing the immediate target, so a link to a link to nothing is
// still reported broken.
QString FileInfoDialog::symlinkTargetString(const QFileInfo &fi) {
    const QString target = fi.symLinkTarget();
    // Unreadable link (permissions on the containing directory); the entry is
    // known to be a link, we just cannot say to what.
    const QString value = target.isEmpty() ? tr("(unknown)") : target;
    return fi.exists() ? value : tr("%1 (broken link)").arg(value);
}

QString FileInfoDialog::permissionsString(const QString &path) {
    const QFile::Permissions p = QFile::permissions(path);
    auto bit = [&](QFile::Permission flag, char set) {
        return QChar((p & flag) ? set : '-');
    };
    QString s;
    s += bit(QFile::ReadOwner, 'r');
    s += bit(QFile::WriteOwner, 'w');
    s += bit(QFile::ExeOwner, 'x');
    s += bit(QFile::ReadGroup, 'r');
    s += bit(QFile::WriteGroup, 'w');
    s += bit(QFile::ExeGroup, 'x');
    s += bit(QFile::ReadOther, 'r');
    s += bit(QFile::WriteOther, 'w');
    s += bit(QFile::ExeOther, 'x');
    return s;
}

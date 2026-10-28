#include "fileinfodialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
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
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gui/customwidgets/datetimepickerpopup.h"
#include "gui/customwidgets/entryinfoitem.h"
#include "settings.h"
#include "sourcecontainers/documentinfo.h"

namespace {

// the only Exif tag with a machine-readable format, so the only one the picker fills
const QLatin1String kDateTimeKey("Exif.Image.DateTime");
// Exif 2.3 §4.6.4; also the display format
const QLatin1String kExifDateTimeFormat("yyyy:MM:dd HH:mm:ss");

// Caps on rows built (widgets are the cost). Exif MakerNotes legitimately run to hundreds of tags, XMP far fewer.
constexpr int kMaxXmpRowsDisplayed = 512;
constexpr int kMaxMetadataRowsDisplayed = 4096;

// full exiv2 keys need a wider name column; applied to every row on the tab so columns align
constexpr int kXmpKeyColumnWidth = 190;

QString formatDateTime(const QDateTime &dt) {
    if(!dt.isValid())
        return QStringLiteral("—"); // em dash: unavailable on this filesystem
    return QLocale().toString(dt, QLocale::ShortFormat);
}

// drawn so it matches the row text color in any theme
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
    // header band and rings make it read as a calendar at 16px
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

    // EXIF tab: shown for formats exiv2 reads Exif from (heif read-only), hidden for kinds that cannot store it
    mExifTab = new QWidget(mTabs);
    auto *exifLayout = new QVBoxLayout(mExifTab);
    // a phone photo has 100+ tags; scroll instead of growing the dialog off screen
    mExifScroll = new QScrollArea(mExifTab);
    mExifScroll->setWidgetResizable(true);
    mExifScroll->setFrameShape(QFrame::NoFrame);
    mExifScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mExifRowsContainer = new QWidget(mExifScroll);
    mExifRowsLayout = new QVBoxLayout(mExifRowsContainer);
    mExifRowsLayout->setContentsMargins(0, 0, 0, 0);
    mExifRowsLayout->setSpacing(0);
    mExifRowsLayout->addStretch(1);
    mExifScroll->setWidget(mExifRowsContainer);
    // setWidget() enables background fill, which would paint the light base under the rows
    mExifScroll->viewport()->setAutoFillBackground(false);
    mExifRowsContainer->setAutoFillBackground(false);
    exifLayout->addWidget(mExifScroll, 1);
    mExifPlaceholder = new QLabel(tr("No metadata"), mExifTab);
    mExifPlaceholder->setAlignment(Qt::AlignCenter);
    mExifPlaceholder->hide();
    exifLayout->addWidget(mExifPlaceholder);
    // shown under the rows: there is no Save button to attach a message to
    mExifError = new QLabel(mExifTab);
    mExifError->setObjectName(QStringLiteral("metadataEditError"));
    mExifError->setWordWrap(true);
    mExifError->hide();
    exifLayout->addWidget(mExifError);
    // same setting as "Show full metadata" in Settings, offered where its effect is visible
    mShowAllTagsCheckBox = new QCheckBox(tr("Show all tags"), mExifTab);
    mShowAllTagsCheckBox->setObjectName(QStringLiteral("showAllTagsCheckBox"));
    connect(mShowAllTagsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        if(checked == settings->showFullMetadata())
            return;
        settings->setShowFullMetadata(checked);
        populateExifTab(mTargetPath);
        updateActionButtons();
    });
    exifLayout->addWidget(mShowAllTagsCheckBox);
    mTabs->addTab(mExifTab, tr("EXIF"));
    mTabs->setTabVisible(mTabs->indexOf(mExifTab), false);

    // XMP tab: read-only shape of the EXIF tab, shown for any format that can hold an XMP packet
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
    // explains read-only fields or a capped list, so neither reads as a bug
    mXmpNotice = new QLabel(mXmpTab);
    mXmpNotice->setObjectName(QStringLiteral("xmpNotice"));
    mXmpNotice->setWordWrap(true);
    mXmpNotice->hide();
    xmpLayout->addWidget(mXmpNotice);
    xmpLayout->addStretch(1);
    mTabs->addTab(mXmpTab, tr("XMP"));
    mTabs->setTabVisible(mTabs->indexOf(mXmpTab), false);

    // ICC tab: a summary of the embedded profile
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

    // One irreversible removal per metadata tab, shown only on its tab; Core confirms all three.
    auto makeRemoveButton = [this](const QString &text, const QString &objectName,
                                   const QString &tip) {
        auto *button = new QPushButton(text, this);
        button->setObjectName(objectName);
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        // never the default: Enter dismisses the window, it must not destroy data
        button->setAutoDefault(false);
        button->setDefault(false);
        return button;
    };
    mRemoveExifButton = makeRemoveButton(
        tr("Remove all Exif"), QStringLiteral("stripMetadataButton"),
        tr("Permanently remove every Exif tag from this file. XMP and the colour profile are not affected."));
    mRemoveXmpButton = makeRemoveButton(
        tr("Remove all XMP"), QStringLiteral("removeXmpButton"),
        tr("Permanently remove every XMP property from this file. Exif and the colour profile are not affected."));
    mRemoveIccButton = makeRemoveButton(
        tr("Remove colour profile"), QStringLiteral("removeIccButton"),
        tr("Permanently remove the embedded ICC colour profile. Exif and XMP are not affected."));
    connect(mRemoveExifButton, &QPushButton::clicked, this, [this]() {
        if(!mTargetPath.isEmpty())
            emit removeAllExifRequested(mTargetPath);
    });
    connect(mRemoveXmpButton, &QPushButton::clicked, this, [this]() {
        if(!mTargetPath.isEmpty())
            emit removeAllXmpRequested(mTargetPath);
    });
    connect(mRemoveIccButton, &QPushButton::clicked, this, [this]() {
        if(!mTargetPath.isEmpty())
            emit removeIccProfileRequested(mTargetPath);
    });

    // checking it reveals the per-row "x"; entering the mode replaces a per-row confirmation
    mRemoveToggle = new QPushButton(tr("Remove properties"), this);
    mRemoveToggle->setObjectName(QStringLiteral("removePropertiesToggle"));
    mRemoveToggle->setCheckable(true);
    mRemoveToggle->setCursor(Qt::PointingHandCursor);
    mRemoveToggle->setAutoDefault(false);
    mRemoveToggle->setDefault(false);
    connect(mRemoveToggle, &QPushButton::toggled, this, [this](bool on) {
        for(EntryInfoItem *row : std::as_const(mCustomXmpRows)) {
            // The trailing blank row has nothing to remove.
            row->setRemovable(on && !mCustomXmpOriginalKeys.value(row).isEmpty());
        }
    });

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->addWidget(mRemoveToggle);
    buttonRow->addStretch(1);
    buttonRow->addWidget(mRemoveExifButton);
    buttonRow->addWidget(mRemoveXmpButton);
    buttonRow->addWidget(mRemoveIccButton);
    layout->addLayout(buttonRow);

    connect(mTabs, &QTabWidget::currentChanged, this, [this]() { updateActionButtons(); });

    const QByteArray geometry = settings->fileInfoDialogGeometry();
    if(!geometry.isEmpty())
        restoreGeometry(geometry);

    clearTarget();
}

void FileInfoDialog::showEvent(QShowEvent *event) {
    // Application-wide while shown: clicks on unfocusable widgets never reach this dialog and
    // leave focus (and the pending edit) in place.
    qApp->installEventFilter(this);
    QDialog::showEvent(event);
}

void FileInfoDialog::hideEvent(QHideEvent *event) {
    qApp->removeEventFilter(this);
    // hide() via Core is the usual dismissal and never triggers closeEvent
    settings->setFileInfoDialogGeometry(saveGeometry());
    QDialog::hideEvent(event);
}

bool FileInfoDialog::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() == QEvent::MouseButtonPress)
        commitFocusedEditor(qobject_cast<QWidget *>(watched));
    // QLineEdit ignores ActiveWindowFocusReason, so commit on deactivation
    else if(watched == this && event->type() == QEvent::WindowDeactivate)
        commitFocusedEditor(nullptr);
    return QDialog::eventFilter(watched, event);
}

// Commits explicitly: on deactivation focus is already gone, so Qt reports nothing. Idempotent.
void FileInfoDialog::commitFocusedEditor(QWidget *clicked) {
    QWidget *focused = focusWidget();
    if(!focused || !isAncestorOf(focused))
        return;
    // A click inside the field being edited is a cursor move, not a commit.
    if(clicked && (clicked == focused || focused->isAncestorOf(clicked)))
        return;
    // a grid row commits as a whole, so moving between its cells is not leaving it
    auto owns = [focused](EntryInfoItem *row) {
        return row && row->isAncestorOf(focused);
    };
    EntryInfoItem *editing = nullptr;
    for(EntryInfoItem *row : std::as_const(mEditableRows)) {
        if(owns(row)) {
            editing = row;
            break;
        }
    }
    if(!editing) {
        for(EntryInfoItem *row : std::as_const(mXmpEditableRows)) {
            if(owns(row)) {
                editing = row;
                break;
            }
        }
    }
    if(!editing) {
        for(EntryInfoItem *row : std::as_const(mCustomXmpRows)) {
            if(owns(row)) {
                if(clicked && row->isAncestorOf(clicked))
                    return;
                editing = row;
                break;
            }
        }
    }
    if(!editing)
        return;
    editing->commitEdit();
    focused->clearFocus();
}

void FileInfoDialog::setTarget(const QString &path) {
    mTargetPath = path;
    // delete mode must not follow the user to the next file
    if(mRemoveToggle->isChecked())
        mRemoveToggle->setChecked(false);
    mHasExif = mHasXmp = mHasIcc = false;
    mExifWritable = mXmpWritable = mIccWritable = false;
    populateGeneralTab(path);
    populateExifTab(path);
    populateXmpTab(path);
    populateIccTab(path);
    updateActionButtons();
}

// Hidden when off-tab or nothing to remove; disabled only when there is something but the file is not writable.
void FileInfoDialog::updateActionButtons() {
    const int current = mTabs->currentIndex();
    const bool onExif = current == mTabs->indexOf(mExifTab);
    const bool onXmp = current == mTabs->indexOf(mXmpTab);
    const bool onIcc = current == mTabs->indexOf(mIccTab);

    mRemoveExifButton->setVisible(onExif && mHasExif);
    mRemoveExifButton->setEnabled(mExifWritable);
    mRemoveXmpButton->setVisible(onXmp && mHasXmp);
    mRemoveXmpButton->setEnabled(mXmpWritable);
    mRemoveIccButton->setVisible(onIcc && mHasIcc);
    mRemoveIccButton->setEnabled(mIccWritable);

    // the toggle belongs to the XMP tab's custom grid
    mRemoveToggle->setVisible(onXmp && mXmpWritable && !mCustomXmpRows.isEmpty() &&
                              mCustomXmpRows.size() > 1);
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
    // exists() resolves links; a dangling link still gets its own rows
    if(path.isEmpty() || (!fi.exists() && !isLink)) {
        mRowsContainer->hide();
        mPlaceholder->show();
        return;
    }
    mPlaceholder->hide();
    mRowsContainer->show();

    // not canonicalized: the entry the user picked is the link itself
    addGeneralRow(tr("Path"), fi.absoluteFilePath());
    if(isLink)
        addGeneralRow(tr("Symlink to"), symlinkTargetString(fi));

    // a broken link has no size, permissions or times; don't show "empty file" values
    if(isLink && !fi.exists())
        return;

    if(fi.isDir()) {
        addGeneralRow(tr("Permissions"), permissionsString(path));
        if(!fi.owner().isEmpty())
            addGeneralRow(tr("Owner"), fi.owner());
        // direct children, non-recursive, hidden entries excluded (like file managers)
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

    // DocumentInfo is cheap to construct for ad hoc queries
    QMap<QString, QString> tags;
    QFileInfo fi(path);
    bool editable = false;
    // The tab depends on the format, not on current writability or existing tags:
    // a stripped photo is exactly where someone wants to add a date back.
    bool canCarryExif = false;
    const bool fullList = settings->showFullMetadata();
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsExif(path)) {
            canCarryExif = true;
            // honors the global metadata verbosity, like Core::showDocument
            tags = fullList ? docInfo.getAllTags() : docInfo.getExifTags();
            // Editable needs a writable format (not heif) and a writable file; editable fields come first.
            editable = DocumentInfo::supportsMetadataEditing(path) && fi.isWritable();
            if(editable) {
                mEditableOriginals = docInfo.getEditableTags();
                for(const QString &key : DocumentInfo::editableTagKeys())
                    addEditableExifRow(key, mEditableOriginals.value(key));
            }
        }
    }

    // Drop both key forms (exiv2 key in full mode, translated label in compact mode) of the editable tags
    // so each appears once, as its field. Labels use QObject's context, as in DocumentInfo::loadExifTags().
    const QStringList compactLabels = {QObject::tr("Make"), QObject::tr("Model"),
                                       QObject::tr("Date/Time"), QObject::tr("UserComment")};
    int shown = 0;
    for(auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
        if(editable && (DocumentInfo::editableTagKeys().contains(it.key()) ||
                        compactLabels.contains(it.key())))
            continue;
        // capped so a crafted file with huge tag counts cannot hang the dialog
        if(shown >= kMaxMetadataRowsDisplayed) {
            addExifRow(tr("Showing %1 of %2 tags").arg(shown).arg(tags.size()), QString());
            break;
        }
        addExifRow(it.key(), it.value());
        ++shown;
    }
    // full exiv2 keys need the wider column; widened for every row so columns align
    if(fullList) {
        for(EntryInfoItem *row : mExifRows)
            row->setNameWidth(kXmpKeyColumnWidth);
    }
    mHasExif = canCarryExif && !tags.isEmpty();
    mExifWritable = editable;

    mTabs->setTabVisible(exifIndex, canCarryExif);
    // An empty-but-capable image says so; kinds without the tab hide it and fall back to General.
    mExifPlaceholder->setVisible(canCarryExif && mExifRows.isEmpty());
    mExifScroll->setVisible(!mExifRows.isEmpty());
    {
        // re-read so a Settings change shows up here
        const QSignalBlocker blocker(mShowAllTagsCheckBox);
        mShowAllTagsCheckBox->setChecked(settings->showFullMetadata());
    }
    mShowAllTagsCheckBox->setVisible(canCarryExif);
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
    // Before the trailing stretch, which keeps short lists at the top.
    mExifRowsLayout->insertWidget(mExifRowsLayout->count() - 1, row);
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
    // Before the trailing stretch, which keeps short lists at the top.
    mExifRowsLayout->insertWidget(mExifRowsLayout->count() - 1, row);
    mExifRows.append(row);
    mEditableRows.insert(key, row);
}

// The picker only adds to the field: typing still works, and an empty value removes the tag.
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
        // start from the field's date when there is one
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
    // setInfo() so the focus-out after the popup does not report the pick twice
    row->setInfo(DocumentInfo::editableTagLabel(kDateTimeKey), text);
    commitEditableTag(kDateTimeKey, text);
}

// Core writes, confirms, reloads and reports (Core::saveMetadataTagAt)
void FileInfoDialog::commitEditableTag(const QString &key, const QString &value) {
    if(mTargetPath.isEmpty())
        return;
    // reject a malformed date here so the message names the format; restore the on-disk value
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

// Wider gate than the EXIF tab: supportsXmp() asks if the container can hold XMP (png can).
void FileInfoDialog::populateXmpTab(const QString &path) {
    clearXmpRows();
    mXmpNotice->hide();
    const int xmpIndex = mTabs->indexOf(mXmpTab);
    const bool wasCurrent = (mTabs->currentIndex() == xmpIndex);

    QMap<QString, QString> tags;
    QMap<QString, QStringList> curated;
    QFileInfo fi(path);
    bool canCarryXmp = false;
    bool editable = false;
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsXmp(path)) {
            canCarryXmp = true;
            tags = docInfo.getXmpTags();
            // format gate and file gate are separate: tiff holds XMP we won't write; read-only files
            editable = DocumentInfo::supportsXmpEditing(path) && fi.isWritable();
            if(editable)
                curated = docInfo.getEditableXmpTags();
        }
    }
    mHasXmp = canCarryXmp && !tags.isEmpty();
    mXmpWritable = editable;

    if(editable) {
        // curated fields first
        for(const QString &key : DocumentInfo::editableXmpKeys())
            addEditableXmpRow(key, curated.value(key));
    }

    // standard then custom, each sorted by key: exiv2's iteration order is neither
    QStringList standardKeys, customKeys;
    for(auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
        // curated keys are already shown as fields
        if(editable && DocumentInfo::editableXmpKeys().contains(it.key()))
            continue;
        (DocumentInfo::isRegisteredXmpKey(it.key()) ? standardKeys : customKeys).append(it.key());
    }
    standardKeys.sort();
    customKeys.sort();

    int capped = 0;
    if(!standardKeys.isEmpty()) {
        addXmpSectionHeader(tr("Standard properties"));
        for(const QString &key : std::as_const(standardKeys)) {
            // cap widgets, not the (cheap) map
            if(mXmpRows.size() >= kMaxXmpRowsDisplayed) {
                capped = standardKeys.size();
                break;
            }
            addXmpRow(key, tags.value(key));
        }
    }

    if(editable || !customKeys.isEmpty()) {
        addXmpSectionHeader(tr("Custom properties"));
        for(const QString &key : std::as_const(customKeys)) {
            if(mCustomXmpRows.size() >= kMaxXmpRowsDisplayed) {
                capped = customKeys.size();
                break;
            }
            addCustomXmpRow(key, tags.value(key), editable);
        }
        // the trailing blank row adds a property, so it is present even with no XMP yet
        if(editable && customKeys.size() < DocumentInfo::kMaxCustomXmpProperties)
            ensureTrailingBlankRow();
        else if(editable)
            showXmpNotice(tr("This file already holds the maximum of %1 custom properties.")
                              .arg(DocumentInfo::kMaxCustomXmpProperties));
    }

    if(capped)
        showXmpNotice(tr("Showing %1 of %2 properties.").arg(kMaxXmpRowsDisplayed).arg(capped));
    else if(canCarryXmp && !DocumentInfo::supportsXmpEditing(path))
        showXmpNotice(tr("%1 metadata is shown read-only; ThumbGrid does not write XMP into %1 files.")
                          .arg(fi.suffix().toUpper()));
    else if(canCarryXmp && !fi.isWritable())
        showXmpNotice(tr("This file is read-only, so its XMP cannot be edited."));

    mTabs->setTabVisible(xmpIndex, canCarryXmp);
    // empty says so, unless there are fields to type into
    mXmpPlaceholder->setVisible(canCarryXmp && mXmpRows.isEmpty() &&
                                mCustomXmpRows.isEmpty() && mXmpEditableRows.isEmpty());
    if(!canCarryXmp && wasCurrent)
        mTabs->setCurrentIndex(mTabs->indexOf(mGeneralTab));
}

void FileInfoDialog::addXmpSectionHeader(const QString &title) {
    auto *header = new QLabel(title, mXmpRowsContainer);
    header->setObjectName(QStringLiteral("xmpSectionHeader"));
    mXmpRowsLayout->addWidget(header);
    mXmpHeaders.append(header);
}

void FileInfoDialog::addEditableXmpRow(const QString &key, const QStringList &values) {
    const auto kind = DocumentInfo::xmpValueKind(key);
    const bool isArray = (kind == DocumentInfo::XmpValueKind::Seq ||
                          kind == DocumentInfo::XmpValueKind::Bag);
    auto *row = new EntryInfoItem(mXmpRowsContainer);
    row->setNameWidth(kXmpKeyColumnWidth);
    row->setEditable(true);
    if(isArray) {
        // one item per line: items may contain commas
        row->setMultiLine(true);
        row->setValuePlaceholder(tr("One per line"));
    }
    row->setInfo(DocumentInfo::editableXmpLabel(key),
                 isArray ? values.join(QLatin1Char('\n'))
                         : (values.isEmpty() ? QString() : values.first()));
    if(kind == DocumentInfo::XmpValueKind::Text && key.endsWith(QLatin1String("Rating")))
        row->setValuePlaceholder(QStringLiteral("0 - 5"));
    connect(row, &EntryInfoItem::valueEdited, this,
            [this, key](const QString &text) { commitEditableXmpTag(key, text); });
    mXmpRowsLayout->addWidget(row);
    // kept out of mXmpRows, which holds only what the file actually carries
    mXmpEditableRows.insert(key, row);
}

EntryInfoItem *FileInfoDialog::addCustomXmpRow(const QString &key, const QString &value,
                                               bool editable) {
    auto *row = new EntryInfoItem(mXmpRowsContainer);
    row->setNameWidth(kXmpKeyColumnWidth);
    row->setNameEditable(editable);
    row->setEditable(editable);
    row->setNamePlaceholder(QStringLiteral("Xmp.prefix.Name"));
    row->setExtraPlaceholder(tr("Namespace URI"));
    row->setInfo(key, value);
    // too long to show in full: committing would write back the elided text
    if(value.length() > DocumentInfo::kMaxXmpValueLengthEdited)
        row->setValueReadOnly(true);
    if(editable) {
        connect(row, &EntryInfoItem::rowEdited, this, [this, row]() { commitCustomXmpRow(row); });
        connect(row, &EntryInfoItem::removeRequested, this, [this, row]() {
            const QString original = mCustomXmpOriginalKeys.value(row);
            if(!original.isEmpty() && !mTargetPath.isEmpty())
                emit customXmpRemoveRequested(mTargetPath, original);
        });
    }
    mXmpRowsLayout->addWidget(row);
    mCustomXmpRows.append(row);
    mCustomXmpOriginalKeys.insert(row, key);
    return row;
}

// exactly one trailing blank row, so there is always somewhere to type
void FileInfoDialog::ensureTrailingBlankRow() {
    if(!mCustomXmpRows.isEmpty() && mCustomXmpRows.last()->isBlank())
        return;
    if(mCustomXmpRows.size() >= DocumentInfo::kMaxCustomXmpProperties)
        return;
    addCustomXmpRow(QString(), QString(), true);
}

// Commits a grid row as a whole: blank is never written, value without key writes nothing,
// key without value writes an empty property, clearing the key erases it.
void FileInfoDialog::commitCustomXmpRow(EntryInfoItem *row) {
    if(mTargetPath.isEmpty())
        return;
    const QString original = mCustomXmpOriginalKeys.value(row);
    const QString key = row->currentName().trimmed();
    const QString value = row->currentValue();

    if(key.isEmpty()) {
        if(original.isEmpty()) {
            // keep the typed value so the user can go back and add the key
            if(!value.trimmed().isEmpty())
                showXmpNotice(tr("A property needs a key, for example Xmp.dc.title."));
            return;
        }
        // The key is the row's identity, so clearing it removes the property.
        emit customXmpRemoveRequested(mTargetPath, original);
        return;
    }

    // registered-schema keys have declared types a free-text cell would corrupt; say where to edit them
    if(DocumentInfo::isRegisteredXmpKey(key)) {
        showXmpNotice(DocumentInfo::editableXmpKeys().contains(key)
                          ? tr("\"%1\" is a standard property - edit it in the field above.").arg(key)
                          : tr("\"%1\" belongs to a standard schema, so it cannot be edited here.").arg(key));
        return;
    }

    // a new prefix needs its namespace URI
    const QStringList parts = key.split(QLatin1Char('.'));
    const QString prefix = (parts.size() == 3 && parts.at(0) == QLatin1String("Xmp"))
                               ? parts.at(1)
                               : QString();
    if(prefix.isEmpty()) {
        showXmpNotice(tr("A key looks like Xmp.prefix.Name."));
        return;
    }
    const bool needsUri = !DocumentInfo::isKnownXmpPrefix(prefix);
    row->setExtraVisible(needsUri);
    if(needsUri && row->currentExtra().trimmed().isEmpty()) {
        showXmpNotice(tr("\"%1\" is a new namespace prefix, so it needs a namespace URI.").arg(prefix));
        return;
    }
    mXmpNotice->hide();
    emit customXmpEditRequested(mTargetPath, original, key, value,
                                needsUri ? row->currentExtra().trimmed() : QString());
}

void FileInfoDialog::commitEditableXmpTag(const QString &key, const QString &text) {
    if(mTargetPath.isEmpty())
        return;
    const auto kind = DocumentInfo::xmpValueKind(key);
    QStringList values;
    if(kind == DocumentInfo::XmpValueKind::Seq || kind == DocumentInfo::XmpValueKind::Bag) {
        // Blank lines dropped, whitespace trimmed, order preserved.
        for(const QString &line : text.split(QLatin1Char('\n'))) {
            const QString item = line.trimmed();
            if(!item.isEmpty())
                values.append(item);
        }
    } else if(!text.isEmpty()) {
        values.append(text);
    }
    emit xmpEditRequested(mTargetPath, key, values);
}

void FileInfoDialog::showXmpNotice(const QString &message) {
    mXmpNotice->setText(message);
    mXmpNotice->show();
}

// As populateXmpTab(), for the embedded colour profile.
void FileInfoDialog::populateIccTab(const QString &path) {
    clearIccRows();
    const int iccIndex = mTabs->indexOf(mIccTab);
    const bool wasCurrent = (mTabs->currentIndex() == iccIndex);

    // kept in DocumentInfo's descriptive order
    QList<QPair<QString, QString>> info;
    QFileInfo fi(path);
    bool canCarryIcc = false;
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if((docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) &&
           DocumentInfo::supportsIccProfile(path)) {
            canCarryIcc = true;
            info = docInfo.getIccProfileInfo();
            mHasIcc = docInfo.hasIccProfile();
            mIccWritable = DocumentInfo::supportsIccEditing(path) && fi.isWritable();
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
    for(EntryInfoItem *row : mCustomXmpRows)
        delete row;
    for(EntryInfoItem *row : mXmpEditableRows)
        delete row;
    for(QLabel *header : mXmpHeaders)
        delete header;
    mXmpRows.clear();
    mCustomXmpRows.clear();
    mXmpHeaders.clear();
    mXmpEditableRows.clear();
    mCustomXmpOriginalKeys.clear();
}

void FileInfoDialog::addXmpRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mXmpRowsContainer);
    // full exiv2 keys need the wider column
    row->setNameWidth(kXmpKeyColumnWidth);
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

// Spells out the target even when dangling; fi.exists() resolves the whole chain,
// so a link to a link to nothing is still broken.
QString FileInfoDialog::symlinkTargetString(const QFileInfo &fi) {
    const QString target = fi.symLinkTarget();
    // unreadable link: we know it is a link, not to what
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

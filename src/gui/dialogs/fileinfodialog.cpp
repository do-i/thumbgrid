#include "fileinfodialog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "gui/customwidgets/entryinfoitem.h"
#include "settings.h"
#include "sourcecontainers/documentinfo.h"

namespace {

QString formatDateTime(const QDateTime &dt) {
    if(!dt.isValid())
        return QStringLiteral("—"); // em dash: unavailable on this filesystem
    return QLocale().toString(dt, QLocale::ShortFormat);
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

    // EXIF tab: rows are (re)built by populateExifTab(); the tab itself is
    // enabled only when there is at least one row to show.
    mExifTab = new QWidget(mTabs);
    auto *exifLayout = new QVBoxLayout(mExifTab);
    mExifRowsContainer = new QWidget(mExifTab);
    mExifRowsLayout = new QVBoxLayout(mExifRowsContainer);
    mExifRowsLayout->setContentsMargins(0, 0, 0, 0);
    mExifRowsLayout->setSpacing(0);
    exifLayout->addWidget(mExifRowsContainer);
    exifLayout->addStretch(1);
    mTabs->addTab(mExifTab, tr("EXIF"));
    mTabs->setTabEnabled(mTabs->indexOf(mExifTab), false);

    // Sits below the tabs, not inside them: it acts on the file as a whole, and
    // keeping it out of the tab stack means it stays visible while the user is
    // reading the very EXIF rows it will delete. Red (#stripMetadataButton,
    // styled with the same danger tokens as the delete confirmations) because it
    // rewrites the file on disk and cannot be undone. Core raises the
    // confirmation - see Core::stripMetadataAt().
    mStripButton = new QPushButton(tr("Strip metadata"), this);
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

    // Edit sits left of Strip: ordinary action before the destructive one, and
    // it is enabled on a different rule - editing needs a format exiv2 can
    // rewrite (jpeg/webp), while stripping works on any strippable image.
    mEditButton = new QPushButton(tr("Edit metadata..."), this);
    mEditButton->setObjectName(QStringLiteral("editMetadataButton"));
    mEditButton->setAutoDefault(false);
    mEditButton->setDefault(false);
    connect(mEditButton, &QPushButton::clicked, this, [this]() {
        if(!mTargetPath.isEmpty())
            emit editMetadataRequested(mTargetPath);
    });

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->addStretch(1);
    buttonRow->addWidget(mEditButton);
    buttonRow->addWidget(mStripButton);
    layout->addLayout(buttonRow);

    const QByteArray geometry = settings->fileInfoDialogGeometry();
    if(!geometry.isEmpty())
        restoreGeometry(geometry);

    clearTarget();
}

void FileInfoDialog::hideEvent(QHideEvent *event) {
    // Dismissed via Core::toggleFileInfoDialog()'s hide() far more often than
    // via the window's close button, so geometry is saved here rather than
    // in closeEvent (which a plain hide() never triggers).
    settings->setFileInfoDialogGeometry(saveGeometry());
    QDialog::hideEvent(event);
}

void FileInfoDialog::setTarget(const QString &path) {
    mTargetPath = path;
    populateGeneralTab(path);
    populateExifTab(path);
    updateStripButton(path);
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
    // Narrower than strippable: writing tags back needs a format exiv2 can
    // rewrite, which is jpeg and webp only.
    mEditButton->setEnabled(strippable && DocumentInfo::supportsMetadataEditing(path));
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
    if(path.isEmpty() || !fi.exists()) {
        mRowsContainer->hide();
        mPlaceholder->show();
        return;
    }
    mPlaceholder->hide();
    mRowsContainer->show();

    if(fi.isDir()) {
        addGeneralRow(tr("Path"), fi.absoluteFilePath());
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
    addGeneralRow(tr("Path"), fi.absoluteFilePath());
    addGeneralRow(tr("Size"), QLocale().formattedDataSize(fi.size()));
    addGeneralRow(tr("Permissions"), permissionsString(path));
    if(!fi.owner().isEmpty())
        addGeneralRow(tr("Owner"), fi.owner());
    addGeneralRow(tr("Created"), formatDateTime(fi.birthTime()));
    addGeneralRow(tr("Modified"), formatDateTime(fi.lastModified()));
}

void FileInfoDialog::populateExifTab(const QString &path) {
    clearExifRows();
    const int exifIndex = mTabs->indexOf(mExifTab);
    const bool wasCurrent = (mTabs->currentIndex() == exifIndex);

    // Constructed locally rather than routed in from Core: DocumentInfo is
    // designed for cheap ad hoc queries on a path (Core itself does this,
    // e.g. Core::isSupportedImage in core.cpp), so there is no heavyweight
    // shared state to reuse here.
    QMap<QString, QString> tags;
    QFileInfo fi(path);
    if(!path.isEmpty() && fi.isFile()) {
        DocumentInfo docInfo(path);
        if(docInfo.type() == DocumentType::STATIC || docInfo.type() == DocumentType::ANIMATED) {
            // Honor the global metadata verbosity toggle the same way the
            // document view does (Core::showDocument).
            tags = settings->showFullMetadata() ? docInfo.getAllTags()
                                                : docInfo.getExifTags();
        }
    }

    if(tags.isEmpty()) {
        mTabs->setTabEnabled(exifIndex, false);
        if(wasCurrent)
            mTabs->setCurrentIndex(mTabs->indexOf(mGeneralTab));
        return;
    }

    QMap<QString, QString>::const_iterator it = tags.constBegin();
    for(; it != tags.constEnd(); ++it)
        addExifRow(it.key(), it.value());
    mTabs->setTabEnabled(exifIndex, true);
}

void FileInfoDialog::clearExifRows() {
    for(EntryInfoItem *row : mExifRows)
        delete row;
    mExifRows.clear();
}

void FileInfoDialog::addExifRow(const QString &name, const QString &value) {
    auto *row = new EntryInfoItem(mExifRowsContainer);
    row->setInfo(name, value);
    mExifRowsLayout->addWidget(row);
    mExifRows.append(row);
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

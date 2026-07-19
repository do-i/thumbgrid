#include "duplicatefinderdialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDateTime>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QTextStream>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include "settings.h"
#include "utils/fileoperations.h"

namespace {

const int THUMB_SIZE = 64;

// Folder list that accepts directory drops from the OS / folder view.
class FolderListWidget : public QListWidget {
public:
    explicit FolderListWidget(QWidget *parent = nullptr) : QListWidget(parent) {
        setAcceptDrops(true);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
protected:
    void dragEnterEvent(QDragEnterEvent *e) override {
        if(e->mimeData()->hasUrls())
            e->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override {
        for(const QUrl &url : e->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if(QFileInfo(path).isDir() && findItems(path, Qt::MatchExactly).isEmpty())
                addItem(path);
        }
        e->acceptProposedAction();
    }
};

QWidget *makeFolderListBox(QListWidget *&list, QWidget *parent) {
    auto *box = new QWidget(parent);
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    list = new FolderListWidget(box);
    list->setMaximumHeight(88);
    layout->addWidget(list, 1);
    auto *buttons = new QVBoxLayout();
    auto *addButton = new QPushButton(QObject::tr("Add..."), box);
    auto *removeButton = new QPushButton(QObject::tr("Remove"), box);
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);
    QObject::connect(addButton, &QPushButton::clicked, box, [list, box] {
        QString dir = QFileDialog::getExistingDirectory(box, QObject::tr("Select folder"));
        if(!dir.isEmpty() && list->findItems(dir, Qt::MatchExactly).isEmpty())
            list->addItem(dir);
    });
    QObject::connect(removeButton, &QPushButton::clicked, box, [list] {
        qDeleteAll(list->selectedItems());
    });
    return box;
}

} // namespace

DuplicateFinderDialog::DuplicateFinderDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Find Duplicates"));
    setModal(false);
    resize(860, 640);

    mModel = new DuplicateResultsModel(this);
    mProxy = new DuplicateResultsProxy(this);
    mProxy->setSourceModel(mModel);

    auto *layout = new QVBoxLayout(this);
    setupModeRow();
    setupSetupZone();
    setupRunRow();
    setupResultsZone();

    connect(&mFinder, &DuplicateFinder::progress, this, &DuplicateFinderDialog::onProgress);
    connect(&mFinder, &DuplicateFinder::matchFound, this, &DuplicateFinderDialog::onMatchFound);
    connect(&mFinder, &DuplicateFinder::finished, this, &DuplicateFinderDialog::onSearchFinished);
    connect(&mThumbnailer, &Thumbnailer::thumbnailReady, this, &DuplicateFinderDialog::onThumbnailReady);
    Q_UNUSED(layout)
}

void DuplicateFinderDialog::setupModeRow() {
    auto *modeRow = new QHBoxLayout();
    modeRow->addWidget(new QLabel(tr("Mode:"), this));
    mModeSingle = new QRadioButton(tr("One image"), this);
    mModeCompare = new QRadioButton(tr("Compare folders"), this);
    mModeWithin = new QRadioButton(tr("Within folders"), this);
    mModeSingle->setChecked(true);
    auto *group = new QButtonGroup(this);
    for(auto *radio : {mModeSingle, mModeCompare, mModeWithin}) {
        group->addButton(radio);
        modeRow->addWidget(radio);
        connect(radio, &QRadioButton::toggled, this, [this](bool on) {
            if(on)
                updateModeUi();
        });
    }
    modeRow->addStretch();
    static_cast<QVBoxLayout *>(layout())->addLayout(modeRow);
}

void DuplicateFinderDialog::setupSetupZone() {
    auto *mainLayout = static_cast<QVBoxLayout *>(layout());

    mSourceStack = new QStackedWidget(this);
    mSourceStack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    // page 0: single image picker
    auto *singlePage = new QWidget(this);
    auto *singleLayout = new QHBoxLayout(singlePage);
    singleLayout->setContentsMargins(0, 0, 0, 0);
    singleLayout->addWidget(new QLabel(tr("Image:"), singlePage));
    mSourceEdit = new QLineEdit(singlePage);
    singleLayout->addWidget(mSourceEdit, 1);
    auto *browse = new QPushButton(tr("Browse..."), singlePage);
    singleLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        QString file = QFileDialog::getOpenFileName(this, tr("Select image"), QString(),
                                                    settings->supportedFormatsFilter());
        if(!file.isEmpty())
            mSourceEdit->setText(file);
    });
    mSourceStack->addWidget(singlePage);
    // page 1: source folder list
    auto *comparePage = new QWidget(this);
    auto *compareLayout = new QVBoxLayout(comparePage);
    compareLayout->setContentsMargins(0, 0, 0, 0);
    compareLayout->addWidget(new QLabel(tr("Source folders (reference):"), comparePage));
    compareLayout->addWidget(makeFolderListBox(mSourceFolders, comparePage));
    mSourceStack->addWidget(comparePage);
    mainLayout->addWidget(mSourceStack);

    mTargetsLabel = new QLabel(tr("Search in:"), this);
    mainLayout->addWidget(mTargetsLabel);
    mainLayout->addWidget(makeFolderListBox(mTargetFolders, this));

    auto *optionsRow = new QHBoxLayout();
    mRecursiveBox = new QCheckBox(tr("Include subfolders"), this);
    mRecursiveBox->setChecked(true);
    optionsRow->addWidget(mRecursiveBox);
    optionsRow->addSpacing(12);
    optionsRow->addWidget(new QLabel(tr("Similarity:"), this));
    mSimilaritySlider = new QSlider(Qt::Horizontal, this);
    mSimilaritySlider->setRange(50, 100);
    mSimilaritySlider->setValue(87);
    mSimilaritySlider->setMinimumWidth(120);
    optionsRow->addWidget(mSimilaritySlider);
    mSimilaritySpin = new QSpinBox(this);
    mSimilaritySpin->setRange(50, 100);
    mSimilaritySpin->setValue(87);
    mSimilaritySpin->setSuffix("%");
    optionsRow->addWidget(mSimilaritySpin);
    connect(mSimilaritySlider, &QSlider::valueChanged, mSimilaritySpin, &QSpinBox::setValue);
    connect(mSimilaritySpin, qOverload<int>(&QSpinBox::valueChanged),
            mSimilaritySlider, &QSlider::setValue);
    optionsRow->addSpacing(12);
    mRotatedBox = new QCheckBox(tr("Match rotated"), this);
    mMirroredBox = new QCheckBox(tr("Match mirrored"), this);
    optionsRow->addWidget(mRotatedBox);
    optionsRow->addWidget(mMirroredBox);
    optionsRow->addStretch();
    mainLayout->addLayout(optionsRow);
    updateModeUi();
}

void DuplicateFinderDialog::setupRunRow() {
    auto *runRow = new QHBoxLayout();
    mStartButton = new QPushButton(tr("Start"), this);
    mStartButton->setObjectName("duplicateFinderStartButton");
    mStartButton->setDefault(true);
    runRow->addWidget(mStartButton);
    mProgressBar = new QProgressBar(this);
    mProgressBar->setTextVisible(false);
    runRow->addWidget(mProgressBar, 1);
    mStatsLabel = new QLabel(tr("Idle"), this);
    runRow->addWidget(mStatsLabel);
    static_cast<QVBoxLayout *>(layout())->addLayout(runRow);
    connect(mStartButton, &QPushButton::clicked, this, &DuplicateFinderDialog::onStartClicked);
}

void DuplicateFinderDialog::setupResultsZone() {
    auto *mainLayout = static_cast<QVBoxLayout *>(layout());
    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    mFilterEdit = new QLineEdit(this);
    mFilterEdit->setPlaceholderText(tr("name or path..."));
    filterRow->addWidget(mFilterEdit, 1);
    filterRow->addWidget(new QLabel(tr("Min similarity:"), this));
    mMinSimilaritySpin = new QSpinBox(this);
    mMinSimilaritySpin->setRange(0, 100);
    mMinSimilaritySpin->setValue(0);
    mMinSimilaritySpin->setSuffix("%");
    filterRow->addWidget(mMinSimilaritySpin);
    mainLayout->addLayout(filterRow);
    connect(mFilterEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { mProxy->setTextFilter(text); });
    connect(mMinSimilaritySpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) { mProxy->setMinSimilarity(value); });

    mTreeView = new QTreeView(this);
    mTreeView->setModel(mProxy);
    mTreeView->setSortingEnabled(true);
    mTreeView->sortByColumn(DuplicateResultsModel::COL_SIMILARITY, Qt::DescendingOrder);
    mTreeView->setIconSize(QSize(THUMB_SIZE, THUMB_SIZE));
    mTreeView->setAlternatingRowColors(true);
    mTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTreeView->header()->setStretchLastSection(true);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_CHECK, 36);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_THUMB, THUMB_SIZE + 12);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_SIMILARITY, 110);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_DIMENSIONS, 100);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_SIZE, 90);
    mTreeView->setColumnWidth(DuplicateResultsModel::COL_NAME, 180);
    mainLayout->addWidget(mTreeView, 1);
    connect(mTreeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        QString path = index.data(DuplicateResultsModel::FilePathRole).toString();
        if(!path.isEmpty())
            emit openFileRequested(path);
    });

    mTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mTreeView, &QTreeView::customContextMenuRequested,
            this, &DuplicateFinderDialog::onResultsContextMenu);
    connect(mTreeView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) { updatePreview(current); });

    setupPreviewAndActions();
}

void DuplicateFinderDialog::setupPreviewAndActions() {
    auto *mainLayout = static_cast<QVBoxLayout *>(layout());

    auto *previewBox = new QGroupBox(tr("Preview"), this);
    auto *previewLayout = new QHBoxLayout(previewBox);
    auto makePane = [&](QLabel *&image, QLabel *&info, const QString &title) {
        auto *pane = new QVBoxLayout();
        pane->addWidget(new QLabel(title, previewBox));
        image = new QLabel(previewBox);
        image->setFixedHeight(150);
        image->setAlignment(Qt::AlignCenter);
        pane->addWidget(image);
        info = new QLabel(previewBox);
        info->setTextFormat(Qt::RichText);
        pane->addWidget(info);
        previewLayout->addLayout(pane, 1);
    };
    makePane(mSourcePreview, mSourceInfo, tr("Reference"));
    makePane(mMatchPreview, mMatchInfo, tr("Match"));
    previewBox->setVisible(false);
    previewBox->setObjectName("duplicatePreviewBox");
    mainLayout->addWidget(previewBox);

    auto *bottomRow = new QHBoxLayout();
    auto *smartSelect = new QPushButton(tr("Smart select"), this);
    auto *smartMenu = new QMenu(smartSelect);
    auto addSmart = [&](const QString &text, DuplicateResultsModel::SmartSelectMode mode) {
        smartMenu->addAction(text, this, [this, mode] { mModel->smartSelect(mode); });
    };
    addSmart(tr("Select all matches"), DuplicateResultsModel::SELECT_ALL);
    addSmart(tr("Keep largest resolution"), DuplicateResultsModel::KEEP_LARGEST_RESOLUTION);
    addSmart(tr("Keep largest file"), DuplicateResultsModel::KEEP_LARGEST_FILE);
    addSmart(tr("Keep newest"), DuplicateResultsModel::KEEP_NEWEST);
    addSmart(tr("Keep oldest"), DuplicateResultsModel::KEEP_OLDEST);
    smartMenu->addSeparator();
    addSmart(tr("Clear selection"), DuplicateResultsModel::CLEAR_SELECTION);
    smartSelect->setMenu(smartMenu);
    bottomRow->addWidget(smartSelect);

    mSelectionLabel = new QLabel(tr("0 selected"), this);
    bottomRow->addWidget(mSelectionLabel);
    bottomRow->addStretch();
    mMoveButton = new QPushButton(tr("Move selected..."), this);
    mMoveButton->setEnabled(false);
    bottomRow->addWidget(mMoveButton);
    mDeleteButton = new QPushButton(tr("Delete selected..."), this);
    mDeleteButton->setObjectName("duplicateFinderDeleteButton");
    mDeleteButton->setEnabled(false);
    bottomRow->addWidget(mDeleteButton);
    mainLayout->addLayout(bottomRow);

    connect(mMoveButton, &QPushButton::clicked, this, &DuplicateFinderDialog::moveChecked);
    connect(mDeleteButton, &QPushButton::clicked, this, &DuplicateFinderDialog::deleteChecked);
    connect(mModel, &DuplicateResultsModel::checkedCountChanged, this, [this](int count) {
        mSelectionLabel->setText(tr("%n selected", nullptr, count));
        mMoveButton->setEnabled(count > 0);
        mDeleteButton->setEnabled(count > 0);
    });
}

void DuplicateFinderDialog::updateModeUi() {
    if(mModeSingle->isChecked()) {
        mSourceStack->setVisible(true);
        mSourceStack->setCurrentIndex(0);
        mTargetsLabel->setText(tr("Search in:"));
    } else if(mModeCompare->isChecked()) {
        mSourceStack->setVisible(true);
        mSourceStack->setCurrentIndex(1);
        mTargetsLabel->setText(tr("Target folders (searched for copies of the sources):"));
    } else {
        mSourceStack->setVisible(false);
        mTargetsLabel->setText(tr("Folders to dedupe:"));
    }
}

void DuplicateFinderDialog::presetFor(const QString &path, const QString &currentDirectory) {
    QFileInfo info(path);
    if(info.isFile()) {
        mModeSingle->setChecked(true);
        mSourceEdit->setText(path);
        if(mTargetFolders->count() == 0 && !currentDirectory.isEmpty())
            mTargetFolders->addItem(currentDirectory);
    } else if(info.isDir()) {
        mModeWithin->setChecked(true);
        if(mTargetFolders->findItems(path, Qt::MatchExactly).isEmpty())
            mTargetFolders->addItem(path);
    } else if(mTargetFolders->count() == 0 && !currentDirectory.isEmpty()) {
        mModeWithin->setChecked(true);
        mTargetFolders->addItem(currentDirectory);
    }
}

DuplicateSearchRequest DuplicateFinderDialog::buildRequest() const {
    DuplicateSearchRequest req;
    if(mModeSingle->isChecked())
        req.mode = DuplicateSearchRequest::SINGLE_IMAGE;
    else if(mModeCompare->isChecked())
        req.mode = DuplicateSearchRequest::COMPARE_FOLDERS;
    else
        req.mode = DuplicateSearchRequest::WITHIN_FOLDERS;
    req.sourceImage = mSourceEdit->text().trimmed();
    req.sourceFolders = foldersIn(mSourceFolders);
    req.targetFolders = foldersIn(mTargetFolders);
    req.recursive = mRecursiveBox->isChecked();
    req.similarityPercent = mSimilaritySpin->value();
    req.matchRotated = mRotatedBox->isChecked();
    req.matchMirrored = mMirroredBox->isChecked();
    return req;
}

bool DuplicateFinderDialog::validateRequest(const DuplicateSearchRequest &request, QString &error) const {
    if(request.mode == DuplicateSearchRequest::SINGLE_IMAGE && !QFileInfo(request.sourceImage).isFile())
        error = tr("Select an image to search for.");
    else if(request.mode == DuplicateSearchRequest::COMPARE_FOLDERS && request.sourceFolders.isEmpty())
        error = tr("Add at least one source folder.");
    else if(request.targetFolders.isEmpty())
        error = tr("Add at least one folder to search in.");
    return error.isEmpty();
}

QStringList DuplicateFinderDialog::foldersIn(const QListWidget *list) {
    QStringList result;
    for(int i = 0; i < list->count(); i++)
        result << list->item(i)->text();
    return result;
}

void DuplicateFinderDialog::onStartClicked() {
    if(mFinder.isRunning()) {
        mFinder.cancel();
        mStartButton->setEnabled(false);
        return;
    }
    DuplicateSearchRequest request = buildRequest();
    QString error;
    if(!validateRequest(request, error)) {
        QMessageBox::warning(this, tr("Find Duplicates"), error);
        return;
    }
    mModel->clear();
    mThumbsRequested.clear();
    mSearchRoots = request.targetFolders + request.sourceFolders;
    mProgressBar->setRange(0, 0);
    mStatsLabel->setText(tr("Scanning..."));
    mStartButton->setText(tr("Cancel"));
    mFinder.start(request);
}

void DuplicateFinderDialog::onProgress(int filesHashed, int filesTotal, int matchesFound) {
    mProgressBar->setRange(0, qMax(1, filesTotal));
    mProgressBar->setValue(filesHashed);
    mStatsLabel->setText(tr("%1 files · %2 hashed · %3 matches")
                             .arg(filesTotal).arg(filesHashed).arg(matchesFound));
}

void DuplicateFinderDialog::onMatchFound(const DuplicateMatch &match) {
    mModel->addMatch(match);
    mTreeView->expand(mProxy->mapFromSource(mModel->groupIndex(match.sourcePath)));
    requestThumbnail(match.sourcePath);
    requestThumbnail(match.matchPath);
}

void DuplicateFinderDialog::requestThumbnail(const QString &path) {
    if(mThumbsRequested.contains(path))
        return;
    mThumbsRequested.insert(path);
    mThumbnailer.getThumbnailAsync(path, THUMB_SIZE, false, false);
}

void DuplicateFinderDialog::onThumbnailReady(const std::shared_ptr<Thumbnail> &thumbnail,
                                             const QString &filePath) {
    if(thumbnail && thumbnail->pixmap())
        mModel->setThumbnail(filePath, *thumbnail->pixmap());
}

void DuplicateFinderDialog::onSearchFinished(bool cancelled) {
    mStartButton->setText(tr("Start"));
    mStartButton->setEnabled(true);
    mProgressBar->setRange(0, 1);
    mProgressBar->setValue(cancelled ? 0 : 1);
    if(cancelled)
        mStatsLabel->setText(tr("Cancelled"));
    else
        mStatsLabel->setText(tr("Done · %n match(es)", nullptr, mModel->matchCount()));
}

void DuplicateFinderDialog::closeEvent(QCloseEvent *event) {
    mFinder.cancel();
    QDialog::closeEvent(event);
}

//------------------------------------------------------------------------------

void DuplicateFinderDialog::deleteChecked() {
    deletePaths(mModel->checkedPaths());
}

void DuplicateFinderDialog::deletePaths(const QStringList &paths) {
    if(paths.isEmpty())
        return;
    qint64 bytes = 0;
    for(const QString &path : paths)
        bytes += QFileInfo(path).size();
    QString msg = tr("Move %n file(s) to trash?", nullptr, paths.count())
                  + QString(" (%1)").arg(QLocale().formattedDataSize(bytes, 1));
    if(QMessageBox::question(this, tr("Move to trash"), msg) != QMessageBox::Yes)
        return;
    QStringList removed, failed;
    for(const QString &path : paths) {
        FileOpResult result;
        FileOperations::moveToTrash(path, result);
        (result == FileOpResult::SUCCESS ? removed : failed) << path;
    }
    mModel->removeMatchesForPaths(removed);
    mTreeView->expandAll();
    mStatsLabel->setText(tr("Moved %n file(s) to trash", nullptr, removed.count()));
    if(!failed.isEmpty())
        QMessageBox::warning(this, tr("Move to trash"),
                             tr("Could not trash %n file(s):", nullptr, failed.count())
                             + "\n" + failed.join("\n"));
}

QString DuplicateFinderDialog::searchRootFor(const QString &path) const {
    for(const QString &root : mSearchRoots)
        if(path.startsWith(QDir(root).absolutePath() + "/"))
            return root;
    return QString();
}

void DuplicateFinderDialog::moveChecked() {
    if(mModel->checkedPaths().isEmpty())
        return;
    QString dest = QFileDialog::getExistingDirectory(this, tr("Move duplicates to..."));
    if(!dest.isEmpty())
        moveCheckedTo(dest);
}

void DuplicateFinderDialog::moveCheckedTo(const QString &dest) {
    QStringList paths = mModel->checkedPaths();
    if(paths.isEmpty() || dest.isEmpty())
        return;
    // timestamped session folder, original relative paths preserved,
    // manifest for auditability (docs/003 §3.3)
    QString session = "dedupe-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    QDir destDir(dest);
    if(!destDir.mkpath(session)) {
        QMessageBox::warning(this, tr("Move duplicates"), tr("Could not create %1").arg(session));
        return;
    }
    QString sessionPath = destDir.filePath(session);
    QFile manifest(sessionPath + "/manifest.txt");
    manifest.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    QTextStream manifestOut(&manifest);
    QStringList moved, failed;
    for(const QString &path : paths) {
        QString root = searchRootFor(path);
        QString rel = root.isEmpty() ? QFileInfo(path).fileName()
                                     : QDir(root).relativeFilePath(path);
        QString target = sessionPath + "/" + rel;
        QDir().mkpath(QFileInfo(target).absolutePath());
        for(int n = 1; QFile::exists(target); n++) {
            QFileInfo ti(sessionPath + "/" + rel);
            target = ti.path() + "/" + ti.completeBaseName() + QString("-%1").arg(n)
                     + (ti.suffix().isEmpty() ? "" : "." + ti.suffix());
        }
        bool ok = QFile::rename(path, target);
        if(!ok)
            ok = QFile::copy(path, target) && QFile::remove(path);
        if(ok) {
            manifestOut << path << " -> " << target << "\n";
            moved << path;
        } else {
            failed << path;
        }
    }
    manifest.close();
    mModel->removeMatchesForPaths(moved);
    mTreeView->expandAll();
    mStatsLabel->setText(tr("Moved %n file(s) to %1", nullptr, moved.count()).arg(session));
    if(!failed.isEmpty())
        QMessageBox::warning(this, tr("Move duplicates"),
                             tr("Could not move %n file(s):", nullptr, failed.count())
                             + "\n" + failed.join("\n"));
}

void DuplicateFinderDialog::onResultsContextMenu(const QPoint &pos) {
    QModelIndex idx = mTreeView->indexAt(pos);
    if(!idx.isValid())
        return;
    QString path = idx.data(DuplicateResultsModel::FilePathRole).toString();
    QMenu menu(this);
    menu.addAction(tr("Open"), this, [this, path] { emit openFileRequested(path); });
    if(idx.parent().isValid())
        menu.addAction(tr("Move to trash"), this, [this, path] { deletePaths({path}); });
    menu.exec(mTreeView->viewport()->mapToGlobal(pos));
}

QString DuplicateFinderDialog::previewInfoHtml(const QString &path, const QSize &otherDims,
                                               qint64 otherSize) {
    QFileInfo info(path);
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize dims = reader.size();
    auto colored = [](const QString &text, qint64 self, qint64 other) {
        if(self > other)
            return QString("<span style='color:#3c9a46;'>%1</span>").arg(text);
        if(self < other)
            return QString("<span style='color:#c05050;'>%1</span>").arg(text);
        return text;
    };
    QString dimsText = colored(QString("%1 x %2").arg(dims.width()).arg(dims.height()),
                               qint64(dims.width()) * dims.height(),
                               qint64(otherDims.width()) * otherDims.height());
    QString sizeText = colored(QLocale().formattedDataSize(info.size(), 1), info.size(), otherSize);
    return QString("<b>%1</b><br>%2 &middot; %3<br>%4")
        .arg(info.fileName().toHtmlEscaped(), dimsText, sizeText,
             QLocale().toString(info.lastModified(), QLocale::ShortFormat));
}

void DuplicateFinderDialog::updatePreview(const QModelIndex &current) {
    QWidget *box = mSourcePreview->parentWidget();
    if(!current.isValid() || !current.parent().isValid()) {
        box->setVisible(false);
        return;
    }
    QString matchPath = current.data(DuplicateResultsModel::FilePathRole).toString();
    QString sourcePath = current.parent().data(DuplicateResultsModel::FilePathRole).toString();
    auto loadInto = [](QLabel *label, const QString &path) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QSize size = reader.size();
        if(size.isValid()) {
            size.scale(280, 150, Qt::KeepAspectRatio);
            reader.setScaledSize(size);
        }
        QImage img = reader.read();
        label->setPixmap(img.isNull() ? QPixmap() : QPixmap::fromImage(img));
    };
    loadInto(mSourcePreview, sourcePath);
    loadInto(mMatchPreview, matchPath);
    QFileInfo sourceInfo(sourcePath), matchInfo(matchPath);
    QImageReader sr(sourcePath), mr(matchPath);
    mSourceInfo->setText(previewInfoHtml(sourcePath, mr.size(), matchInfo.size()));
    mMatchInfo->setText(previewInfoHtml(matchPath, sr.size(), sourceInfo.size()));
    box->setVisible(true);
}

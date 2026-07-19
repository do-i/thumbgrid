#pragma once

#include <QDialog>
#include <QSet>
#include "components/duplicatefinder/duplicatefinder.h"
#include "components/thumbnailer/thumbnailer.h"
#include "duplicateresultsmodel.h"

class QButtonGroup;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QStackedWidget;
class QTreeView;

// Non-modal duplicate search window (docs/003 §3.2): setup zone, run row
// with live counters, grouped results tree.
class DuplicateFinderDialog : public QDialog {
    Q_OBJECT
public:
    explicit DuplicateFinderDialog(QWidget *parent = nullptr);

    // Seed from the context the dialog was opened in: a file path selects
    // single-image mode, a directory selects within-folders mode.
    void presetFor(const QString &path, const QString &currentDirectory);
    // Grid-view seeding: one folder -> within-folders; several folders ->
    // compare-folders (first = source, rest = targets); files -> single image.
    void presetForSelection(const QStringList &paths, const QString &currentDirectory);

    // Moves the checked matches into a timestamped session subfolder of
    // dest, preserving search-relative paths, and writes a manifest.
    void moveCheckedTo(const QString &dest);

    // test access
    DuplicateResultsModel *resultsModel() { return mModel; }
    DuplicateFinder *finder() { return &mFinder; }
    QTreeView *resultsView() { return mTreeView; }

signals:
    void openFileRequested(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStartClicked();
    void onProgress(int filesHashed, int filesTotal, int matchesFound);
    void onMatchFound(const DuplicateMatch &match);
    void onSearchFinished(bool cancelled);
    void onThumbnailReady(const std::shared_ptr<Thumbnail> &thumbnail, const QString &filePath);
    void deleteChecked();
    void moveChecked();
    void onResultsContextMenu(const QPoint &pos);
    void updatePreview(const QModelIndex &current);

private:
    void setupModeRow();
    void setupSetupZone();
    void setupRunRow();
    void setupResultsZone();
    void setupPreviewAndActions();
    void updateModeUi();
    void updateStartEnabled();
    void deletePaths(const QStringList &paths);
    QString searchRootFor(const QString &path) const;
    static QString previewInfoHtml(const QString &path, const QSize &otherDims, qint64 otherSize);
    void requestThumbnail(const QString &path);
    DuplicateSearchRequest buildRequest() const;
    bool validateRequest(const DuplicateSearchRequest &request, QString &error) const;
    static QStringList foldersIn(const QListWidget *list);

    DuplicateFinder mFinder;
    Thumbnailer mThumbnailer;
    DuplicateResultsModel *mModel;
    DuplicateResultsProxy *mProxy;
    QSet<QString> mThumbsRequested;

    QRadioButton *mModeSingle = nullptr, *mModeCompare = nullptr, *mModeWithin = nullptr;
    QStackedWidget *mSourceStack = nullptr;
    QLineEdit *mSourceEdit = nullptr;
    QListWidget *mSourceFolders = nullptr;
    QListWidget *mTargetFolders = nullptr;
    QLabel *mTargetsLabel = nullptr;
    QCheckBox *mRecursiveBox = nullptr, *mRotatedBox = nullptr, *mMirroredBox = nullptr;
    QSlider *mSimilaritySlider = nullptr;
    QSpinBox *mSimilaritySpin = nullptr;
    QPushButton *mStartButton = nullptr;
    QPushButton *mClearButton = nullptr;
    QProgressBar *mProgressBar = nullptr;
    QLabel *mStatsLabel = nullptr;
    QLineEdit *mFilterEdit = nullptr;
    QSpinBox *mMinSimilaritySpin = nullptr;
    QTreeView *mTreeView = nullptr;
    QLabel *mSelectionLabel = nullptr;
    QPushButton *mMoveButton = nullptr, *mDeleteButton = nullptr;
    QLabel *mSourcePreview = nullptr, *mMatchPreview = nullptr,
           *mSourceInfo = nullptr, *mMatchInfo = nullptr;
    QStringList mSearchRoots;
};

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

private:
    void setupModeRow();
    void setupSetupZone();
    void setupRunRow();
    void setupResultsZone();
    void updateModeUi();
    void requestThumbnail(const QString &path);
    DuplicateSearchRequest buildRequest() const;
    bool validateRequest(const DuplicateSearchRequest &request, QString &error) const;
    static QStringList foldersIn(const QListWidget *list);

    DuplicateFinder mFinder;
    Thumbnailer mThumbnailer;
    DuplicateResultsModel *mModel;
    DuplicateResultsProxy *mProxy;
    QSet<QString> mThumbsRequested;

    QRadioButton *mModeSingle, *mModeCompare, *mModeWithin;
    QStackedWidget *mSourceStack;
    QLineEdit *mSourceEdit;
    QListWidget *mSourceFolders;
    QListWidget *mTargetFolders;
    QLabel *mTargetsLabel;
    QCheckBox *mRecursiveBox, *mRotatedBox, *mMirroredBox;
    QSlider *mSimilaritySlider;
    QSpinBox *mSimilaritySpin;
    QPushButton *mStartButton;
    QProgressBar *mProgressBar;
    QLabel *mStatsLabel;
    QLineEdit *mFilterEdit;
    QSpinBox *mMinSimilaritySpin;
    QTreeView *mTreeView;
    QLabel *mSelectionLabel;
};

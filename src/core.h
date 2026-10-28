#pragma once

#include <QObject>
#include <QDebug>
#include <QMutex>
#include <QClipboard>
#include <QDrag>
#include <QFileSystemModel>
#include <QDesktopServices>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTranslator>
#include <cstdint>
#include <memory>
#include "appversion.h"
#include "settings.h"
#include "components/directorymodel.h"
#include "components/directorypresenter.h"
#include "components/fileoperationscontroller.h"
#include "components/scriptmanager/scriptmanager.h"
#include "gui/mainwindow.h"
#include "utils/randomizer.h"
#include "gui/dialogs/printdialog.h"
#include "gui/dialogs/duplicatefinderdialog.h"
#include "gui/dialogs/fileinfodialog.h"

#ifdef __GLIBC__
#include <malloc.h>
#endif

struct State {
    bool hasActiveImage = false;
    bool delayModel = false;
    QString currentFilePath = "";
    QString directoryPath = "";
    std::shared_ptr<Image> currentImg;
};

enum MimeDataTarget : std::uint8_t {
    TARGET_CLIPBOARD,
    TARGET_DROP
};

class Core : public QObject {
    Q_OBJECT
public:
    Core();
    void showGui();

public slots:
    void updateInfoString();
    bool loadPath(QString);

private:
    QElapsedTimer t;

    void initGui();
    void initComponents();
    void connectComponents();
    void initActions();
    void loadTranslation();
    void onUpdate();
    void onFirstRun();

    // ui stuff
    MW *mw;

    State state;
    bool loopSlideshow, slideshow, shuffle;
    bool mShowOtherFileTypes = false;
    FolderEndAction folderEndAction;

    // components
    std::shared_ptr<DirectoryModel> model;

    DirectoryPresenter thumbPanelPresenter, folderViewPresenter;
    FileOperationsController *fileOps = nullptr;
    std::unique_ptr<DuplicateFinderDialog> duplicateFinderDialog;
    std::unique_ptr<FileInfoDialog> fileInfoDialog;
    // Retargets the live-follow File info popup, but only while it exists and
    // is visible, so a closed popup costs nothing.
    void retargetFileInfoDialog();

    void rotateByDegrees(int degrees);
    void reset();
    bool setDirectory(const QString& path);

    QDrag *mDrag;
    QMimeData *getMimeDataForImage(const std::shared_ptr<Image>& img, MimeDataTarget target);
    // session-lifetime export dir: receivers read the file after getMimeDataForImage() returns
    std::unique_ptr<QTemporaryDir> mExportTmpDir;
    // Keeps other instances' startup sweep away. Declared after the dir so it is released first.
    std::unique_ptr<QLockFile> mExportTmpLock;
    int mExportFileCounter = 0;
    void copySelectionToClipboard(bool cut);
    QTranslator *translator = nullptr;

    Randomizer randomizer;
    void syncRandomizer();

    void attachModel(DirectoryModel *_model);
    QString selectedPath();
    void guiSetImage(const std::shared_ptr<Image>& img);
    QTimer slideshowTimer;

    void startSlideshowTimer();
    void startSlideshow();
    void stopSlideshow();

    void syncSaveOverlay();
    void syncSaveOverlay(const std::shared_ptr<Image>& img);

    std::shared_ptr<ImageStatic> getEditableImage(const QString &filePath);
    QStringList currentSelection();

    template<typename... Args>
    void edit_template(const std::function<std::unique_ptr<QImage>(std::shared_ptr<const QImage>, Args...)>& func, Args&&... as);


private slots:
    void readSettings();
    void nextImage();
    void prevImage();
    void nextImageSlideshow();
    void jumpToFirst();
    void jumpToLast();
    void onModelItemReady(const std::shared_ptr<Image>&, const QString&);
    void onModelItemUpdated(const QString& fileName);
    void onModelSortingChanged(SortingMode mode);
    void onLoadFailed(const QString &path);
    void rotateLeft();
    void rotateRight();
    void close();
    void scalingRequest(QSize, ScalingFilter);
    void onScalingFinished(QPixmap* scaled, const ScalerRequest& req);
    void copyCurrentFile(const QString& destDirectory);
    void moveCurrentFile(const QString& destDirectory);
    void moveSelection();
    void convertSelectionToFormat(QString format);
    FileOpResult removeFile(const QString& fileName, bool trash);
    void onFileRemoved(const QString& filePath, int index);
    void onFileRenamed(const QString& fromPath, int indexFrom, const QString& toPath, int indexTo);
    void onFileAdded(const QString& filePath);
    void onFileModified(const QString& filePath);
    void onSaveFinished(const QString& filePath, const QString& destPath, bool success);
    void showResizeDialog();
    void resize(QSize size);
    void resizeSelectionToCopies(ResizeSpec spec);
    void flipH();
    void flipV();
    void crop(QRect rect);
    void discardEdits();
    void toggleCropPanel();
    void toggleFullscreenInfoBar();
    void toggleStatusFooter();
    void requestSavePath();
    void saveCurrentFile();
    void saveCurrentFileAs(const QString&);
    void runScript(const QString&);
    void setWallpaper();
    void removePermanent();
    void moveToTrash();
    void reloadImage();
    void reloadImage(QString fileName);
    void stripMetadata();
    void stripMetadataAt(const QString &path);
    void saveMetadataTagAt(const QString &path, const QString &key, const QString &value);
    // scoped removals, one per File info tab, each like stripMetadataAt() for its own kind
    void removeAllExifAt(const QString &path);
    void removeAllXmpAt(const QString &path);
    void removeIccProfileAt(const QString &path);
    void saveXmpTagAt(const QString &path, const QString &key, const QStringList &values);
    void saveCustomXmpAt(const QString &path, const QString &oldKey, const QString &newKey,
                         const QString &value, const QString &namespaceUri);
    void removeCustomXmpAt(const QString &path, const QString &key);

private:
    // The one call the three scoped removals differ by.
    using MetadataRemoval = bool (DocumentInfo::*)();
    void runScopedMetadataRemoval(const QString &path, const QString &title,
                                  const QString &prompt, MetadataRemoval removal,
                                  const QString &okMessage, const QString &failMessage);

public slots:
    void copyFileClipboard();
    void cutFileClipboard();
    void pasteFile();
    void copyPathClipboard();
    void openFromClipboard();
    void renameCurrentSelection(const QString& newName);
    void searchFolderView(const QString& prefix);
    void sortBy(SortingMode mode);
    void sortByName();
    void sortByTime();
    void sortBySize();
    void showRenameDialog();
    void showDuplicateFinder();
    void showFileInfoDialog();
    void toggleFileInfoDialog();
    void createDirectory();
    void onDraggedOut();
    void onDraggedOut(QStringList paths);
    void onDropIn(const QMimeData *mimeData, QObject* source);
    void toggleShuffle();
    void onModelLoaded();
    void outputError(const FileOpResult &error) const;
    void showOpenDialog();
    void showInDirectory();
    void onDirectoryViewFileActivated(const QString& filePath);
    bool loadFileIndex(int index, bool async, bool preload);
    bool canDisplayFile(const QString &path) const;
    int nearestViewableIndex(int from, int step) const;
    int nextShuffledViewable(bool forward);
    void enableDocumentView();
    void enableFolderView();
    void toggleFolderView();
    void toggleFolderViewTopBar();
    void openSelected();
    void selectAll();
    void startSearch();
    void navigateGrid(GridNavigation direction);
    void togglePlacesPanel();
    void toggleSlideshow();
    void onPlaybackFinished();
    void setFoldersDisplay(bool mode);
    void loadParentDir();
    void nextDirectory();
    void prevDirectory(bool selectLast);
    void prevDirectory();
    void print();
    void modelDelayLoad();
};

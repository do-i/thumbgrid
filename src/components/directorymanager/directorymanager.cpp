#include "directorymanager.h"

#include <utility>
#include "utils/logging.h"
#include "utils/pathstring.h"

namespace fs = std::filesystem;

namespace {
// Stats a scanned directory_entry as a non-directory file and fills out `out`.
// Returns false (leaving `out` untouched) if the entry vanished mid-scan.
// Shared by addEntriesFromDirectory() and addEntriesFromDirectoryRecursive().
bool tryMakeFileEntry(const fs::directory_entry &entry, const QString &name, const QString &path, FSEntry &out) {
    std::error_code ec;
    std::uintmax_t size = entry.file_size(ec);
    if(ec)
        return false;
    auto modifyTime = entry.last_write_time(ec);
    if(ec)
        return false;
    out.name = name;
    out.path = path;
    out.isDirectory = false;
    out.size = size;
    out.modifyTime = modifyTime;
    return true;
}
}

DirectoryManager::DirectoryManager() :
    watcher(nullptr),
    mSortingMode(SORT_NAME)
{
    regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    collator.setNumericMode(true);

    readSettings();
    setSortingMode(settings->sortingMode());
    connect(settings, &Settings::settingsChanged, this, &DirectoryManager::readSettings);
}

template< typename T, typename Pred >
typename std::vector<T>::iterator
insert_sorted(std::vector<T> & vec, T const& item, Pred pred) {
    return vec.insert(std::upper_bound(vec.begin(), vec.end(), item, pred), item);
}

bool DirectoryManager::path_entry_compare(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.path, e2.path) < 0;
};

bool DirectoryManager::path_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.path, e2.path) > 0;
};

bool DirectoryManager::name_entry_compare(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.name, e2.name) < 0;
};

bool DirectoryManager::name_entry_compare_reverse(const FSEntry &e1, const FSEntry &e2) const {
    return collator.compare(e1.name, e2.name) > 0;
};

bool DirectoryManager::date_entry_compare(const FSEntry& e1, const FSEntry& e2) const {
    return e1.modifyTime < e2.modifyTime;
}

bool DirectoryManager::date_entry_compare_reverse(const FSEntry& e1, const FSEntry& e2) const {
    return e1.modifyTime > e2.modifyTime;
}

bool DirectoryManager::size_entry_compare(const FSEntry& e1, const FSEntry& e2) const {
    return e1.size < e2.size;
}

bool DirectoryManager::size_entry_compare_reverse(const FSEntry& e1, const FSEntry& e2) const {
    return e1.size > e2.size;
}

DirectoryManager::CompareFn DirectoryManager::compareFunction() {
    switch(mSortingMode) {
    case SortingMode::SORT_NAME_DESC:
        return [this](const FSEntry &e1, const FSEntry &e2) { return path_entry_compare_reverse(e1, e2); };
    case SortingMode::SORT_TIME:
        return [this](const FSEntry &e1, const FSEntry &e2) { return date_entry_compare(e1, e2); };
    case SortingMode::SORT_TIME_DESC:
        return [this](const FSEntry &e1, const FSEntry &e2) { return date_entry_compare_reverse(e1, e2); };
    case SortingMode::SORT_SIZE:
        return [this](const FSEntry &e1, const FSEntry &e2) { return size_entry_compare(e1, e2); };
    case SortingMode::SORT_SIZE_DESC:
        return [this](const FSEntry &e1, const FSEntry &e2) { return size_entry_compare_reverse(e1, e2); };
    default:
        return [this](const FSEntry &e1, const FSEntry &e2) { return path_entry_compare(e1, e2); };
    }
}

void DirectoryManager::startFileWatcher(const QString& directoryPath) {
    if(directoryPath == "")
        return;
    if(!watcher)
        watcher = DirectoryWatcher::newInstance();

    connect(watcher, &DirectoryWatcher::fileCreated,  this, &DirectoryManager::onFileAddedExternal,    Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileDeleted,  this, &DirectoryManager::onFileRemovedExternal,  Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileModified, this, &DirectoryManager::onFileModifiedExternal, Qt::UniqueConnection);
    connect(watcher, &DirectoryWatcher::fileRenamed,  this, &DirectoryManager::onFileRenamedExternal,  Qt::UniqueConnection);

    watcher->setWatchPath(directoryPath);
    watcher->observe();
}

void DirectoryManager::stopFileWatcher() {
    if(!watcher)
        return;

    watcher->stopObserving();

    disconnect(watcher, &DirectoryWatcher::fileCreated,  this, &DirectoryManager::onFileAddedExternal);
    disconnect(watcher, &DirectoryWatcher::fileDeleted,  this, &DirectoryManager::onFileRemovedExternal);
    disconnect(watcher, &DirectoryWatcher::fileModified, this, &DirectoryManager::onFileModifiedExternal);
    disconnect(watcher, &DirectoryWatcher::fileRenamed,  this, &DirectoryManager::onFileRenamedExternal);
}

// ##############################################################
// ####################### PUBLIC METHODS #######################
// ##############################################################

void DirectoryManager::readSettings() {
    regex.setPattern(settings->supportedFormatsRegex());
    mIncludeOtherFiles = settings->showOtherFileTypes();
}

bool DirectoryManager::validateDirectory(const QString& dirPath) const {
    if(dirPath.isEmpty()) {
        return false;
    }
    if(!std::filesystem::exists(toStdString(dirPath))) {
        qCWarning(logDirManager) << "[DirectoryManager] Error - path does not exist.";
        return false;
    }
    if(!std::filesystem::is_directory(toStdString(dirPath))) {
        qCWarning(logDirManager) << "[DirectoryManager] Error - path is not a directory.";
        return false;
    }
    return true;
}

bool DirectoryManager::setDirectory(const QString& dirPath) {
    if(!validateDirectory(dirPath))
        return false;
    QDir dir(dirPath);
    if(!dir.isReadable()) {
        qCWarning(logDirManager) << "[DirectoryManager] Error - cannot read directory.";
        return false;
    }
    mListSource = SOURCE_DIRECTORY;
    mDirectoryPath = dirPath;

    loadEntryList(dirPath, false);
    sortEntryLists();
    emit loaded(dirPath);
    startFileWatcher(dirPath);
    return true;
}

bool DirectoryManager::setDirectoryRecursive(const QString& dirPath) {
    if(!validateDirectory(dirPath))
        return false;
    stopFileWatcher();
    mListSource = SOURCE_DIRECTORY_RECURSIVE;
    mDirectoryPath = dirPath;
    loadEntryList(dirPath, true);
    sortEntryLists();
    emit loaded(dirPath);
    return true;
}

QString DirectoryManager::directoryPath() const {
    if(mListSource == SOURCE_DIRECTORY || mListSource == SOURCE_DIRECTORY_RECURSIVE)
        return mDirectoryPath;
    else
        return "";
}

int DirectoryManager::indexOfFile(const QString& filePath) const {
    ensureFileIndexCache();
    return fileIndexCache.value(filePath, -1);
}

int DirectoryManager::indexOfDir(const QString& dirPath) const {
    ensureDirIndexCache();
    return dirIndexCache.value(dirPath, -1);
}

void DirectoryManager::rebuildFileIndexCache() const {
    fileIndexCache.clear();
    fileIndexCache.reserve((int)fileEntryVec.size());
    for(int i = 0; i < (int)fileEntryVec.size(); i++)
        fileIndexCache.insert(fileEntryVec.at(i).path, i);
    fileIndexCacheDirty = false;
}

void DirectoryManager::rebuildDirIndexCache() const {
    dirIndexCache.clear();
    dirIndexCache.reserve((int)dirEntryVec.size());
    for(int i = 0; i < (int)dirEntryVec.size(); i++)
        dirIndexCache.insert(dirEntryVec.at(i).path, i);
    dirIndexCacheDirty = false;
}

void DirectoryManager::ensureFileIndexCache() const {
    if(fileIndexCacheDirty)
        rebuildFileIndexCache();
}

void DirectoryManager::ensureDirIndexCache() const {
    if(dirIndexCacheDirty)
        rebuildDirIndexCache();
}

QString DirectoryManager::filePathAt(int index) const {
    return checkFileRange(index) ? fileEntryVec.at(index).path : "";
}

QString DirectoryManager::fileNameAt(int index) const {
    return checkFileRange(index) ? fileEntryVec.at(index).name : "";
}

QString DirectoryManager::dirPathAt(int index) const {
    return checkDirRange(index) ? dirEntryVec.at(index).path : "";
}

QString DirectoryManager::dirNameAt(int index) const {
    return checkDirRange(index) ? dirEntryVec.at(index).name : "";
}

QString DirectoryManager::firstFile() const {
    QString filePath = "";
    if(fileEntryVec.size())
        filePath = fileEntryVec.front().path;
    return filePath;
}

QString DirectoryManager::lastFile() const {
    QString filePath = "";
    if(fileEntryVec.size())
        filePath = fileEntryVec.back().path;
    return filePath;
}

QString DirectoryManager::prevOfFile(QString filePath) const {
    QString prevFilePath = "";
    int currentIndex = indexOfFile(std::move(filePath));
    if(currentIndex > 0)
        prevFilePath = fileEntryVec.at(currentIndex - 1).path;
    return prevFilePath;
}

QString DirectoryManager::nextOfFile(QString filePath) const {
    QString nextFilePath = "";
    int currentIndex = indexOfFile(std::move(filePath));
    if(currentIndex >= 0 && currentIndex < fileEntryVec.size() - 1)
        nextFilePath = fileEntryVec.at(currentIndex + 1).path;
    return nextFilePath;
}

QString DirectoryManager::prevOfDir(QString dirPath) const {
    QString prevDirectoryPath = "";
    int currentIndex = indexOfDir(std::move(dirPath));
    if(currentIndex > 0)
        prevDirectoryPath = dirEntryVec.at(currentIndex - 1).path;
    return prevDirectoryPath;
}

QString DirectoryManager::nextOfDir(QString dirPath) const {
    QString nextDirectoryPath = "";
    int currentIndex = indexOfDir(std::move(dirPath));
    if(currentIndex >= 0 && currentIndex < dirEntryVec.size() - 1)
        nextDirectoryPath = dirEntryVec.at(currentIndex + 1).path;
    return nextDirectoryPath;
}

bool DirectoryManager::checkFileRange(int index) const {
    return index >= 0 && index < (int)fileEntryVec.size();
}

bool DirectoryManager::checkDirRange(int index) const {
    return index >= 0 && index < (int)dirEntryVec.size();
}

unsigned long DirectoryManager::totalCount() const {
    return fileCount() + dirCount();
}

unsigned long DirectoryManager::fileCount() const {
    return fileEntryVec.size();
}

unsigned long DirectoryManager::dirCount() const {
    return dirEntryVec.size();
}

const FSEntry &DirectoryManager::fileEntryAt(int index) const {
    if(checkFileRange(index))
        return fileEntryVec.at(index);
    else
        return defaultEntry;
}

QDateTime DirectoryManager::lastModified(const QString& filePath) const {
    QFileInfo info;
    if(containsFile(filePath))
        info.setFile(filePath);
    return info.lastModified();
}

// TODO: what about symlinks?
inline
bool DirectoryManager::isSupportedFile(const QString& path) const {
    return ( isFile(path) && (mIncludeOtherFiles || regex.match(path).hasMatch()) );
}

bool DirectoryManager::isFile(const QString& path) const {
    std::error_code ec;
    auto st = fs::status(toStdString(path), ec);
    return !ec && fs::is_regular_file(st);
}

bool DirectoryManager::isDir(const QString& path) const {
    std::error_code ec;
    auto st = fs::status(toStdString(path), ec);
    return !ec && fs::is_directory(st);
}

bool DirectoryManager::isEmpty() const {
    return fileEntryVec.empty();
}

bool DirectoryManager::containsFile(const QString& filePath) const {
    ensureFileIndexCache();
    return fileIndexCache.contains(filePath);
}

bool DirectoryManager::containsDir(const QString& dirPath) const {
    ensureDirIndexCache();
    return dirIndexCache.contains(dirPath);
}

// ##############################################################
// ###################### PRIVATE METHODS #######################
// ##############################################################
void DirectoryManager::loadEntryList(const QString& directoryPath, bool recursive) {
    dirEntryVec.clear();
    fileEntryVec.clear();
    if(recursive) { // load files only
        addEntriesFromDirectoryRecursive(fileEntryVec, directoryPath);
    } else { // load dirs & files
        addEntriesFromDirectory(fileEntryVec, directoryPath);
    }
    fileIndexCacheDirty = true;
    dirIndexCacheDirty = true;
}

// both directories & files
void DirectoryManager::addEntriesFromDirectory(std::vector<FSEntry> &entryVec, const QString& directoryPath) {
    QRegularExpressionMatch match;
    std::error_code ec;
    fs::directory_iterator it(toStdString(directoryPath), fs::directory_options::skip_permission_denied, ec);
    if(ec) {
        qCWarning(logDirManager) << "[DirectoryManager]" << directoryPath << QString::fromStdString(ec.message());
        return;
    }
    for(fs::directory_iterator end; it != end; it.increment(ec)) {
        if(ec) { // increment failure invalidates the iterator; nothing more to read
            qCWarning(logDirManager) << "[DirectoryManager]" << QString::fromStdString(ec.message());
            break;
        }
        const auto &entry = *it;
        QString name = QString::fromStdString(entry.path().filename().generic_string());
#ifndef Q_OS_WIN32
        // ignore hidden files
        if(!settings->showHiddenFiles() && name.startsWith("."))
            continue;
#else
        DWORD attributes = GetFileAttributes(entry.path().generic_string().c_str());
        if(!settings->showHiddenFiles() && attributes & FILE_ATTRIBUTE_HIDDEN)
            continue;
#endif
        QString path = QString::fromStdString(entry.path().generic_string());
        match = regex.match(name);
        std::error_code entryEc;
        if(entry.is_directory(entryEc) && !entryEc) {
            FSEntry newEntry;
            newEntry.name = name;
            newEntry.path = path;
            newEntry.isDirectory = true;
            dirEntryVec.emplace_back(newEntry);
        } else if(match.hasMatch() || mIncludeOtherFiles) {
            // the file may vanish mid-scan; skip it instead of aborting the whole listing
            FSEntry newEntry;
            if(tryMakeFileEntry(entry, name, path, newEntry))
                entryVec.emplace_back(newEntry);
        }
    }
}

void DirectoryManager::addEntriesFromDirectoryRecursive(std::vector<FSEntry> &entryVec, const QString& directoryPath) {
    QRegularExpressionMatch match;
    std::error_code ec;
    fs::recursive_directory_iterator it(toStdString(directoryPath), fs::directory_options::skip_permission_denied, ec);
    if(ec) {
        qCWarning(logDirManager) << "[DirectoryManager]" << directoryPath << QString::fromStdString(ec.message());
        return;
    }
    for(fs::recursive_directory_iterator end; it != end; it.increment(ec)) {
        if(ec) {
            qCWarning(logDirManager) << "[DirectoryManager]" << QString::fromStdString(ec.message());
            break;
        }
        const auto &entry = *it;
        QString name = QString::fromStdString(entry.path().filename().generic_string());
        QString path = QString::fromStdString(entry.path().generic_string());
        match = regex.match(name);
        std::error_code entryEc;
        if(!entry.is_directory(entryEc) && !entryEc && (match.hasMatch() || mIncludeOtherFiles)) {
            FSEntry newEntry;
            if(tryMakeFileEntry(entry, name, path, newEntry))
                entryVec.emplace_back(newEntry);
        }
    }
}

void DirectoryManager::sortEntryLists() {
    if(settings->sortFolders())
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), compareFunction());
    else
        std::sort(dirEntryVec.begin(), dirEntryVec.end(), [this](const FSEntry &e1, const FSEntry &e2) { return path_entry_compare(e1, e2); });
    std::sort(fileEntryVec.begin(), fileEntryVec.end(), compareFunction());
    fileIndexCacheDirty = true;
    dirIndexCacheDirty = true;
}

void DirectoryManager::setSortingMode(SortingMode mode) {
    if(mode != mSortingMode) {
        mSortingMode = mode;
        if(fileEntryVec.size() > 1 || dirEntryVec.size() > 1) {
            sortEntryLists();
            emit sortingChanged();
        }
    }
}

SortingMode DirectoryManager::sortingMode() const {
    return mSortingMode;
}

// Entry management

bool DirectoryManager::insertFileEntry(const QString &filePath) {
    if(!isSupportedFile(filePath))
        return false;
    return forceInsertFileEntry(filePath);
}

// stat()s filePath and builds the resulting FSEntry; the file can vanish
// between the caller's isFile() check and these stat calls (fs watcher races),
// so failures are tolerated and just fall back to a size/time of 0.
FSEntry DirectoryManager::statFileEntry(const QString &filePath, const QString &fileName) const {
    std::error_code ec;
    std::uintmax_t size = fs::file_size(toStdString(filePath), ec);
    if(ec)
        size = 0;
    auto modifyTime = fs::last_write_time(toStdString(filePath), ec);
    if(ec)
        modifyTime = fs::file_time_type();
    return FSEntry(filePath, fileName, size, modifyTime, false);
}

void DirectoryManager::insertFileEntrySorted(const FSEntry &entry) {
    insert_sorted(fileEntryVec, entry, compareFunction());
    fileIndexCacheDirty = true;
}

// skips filename regex check
bool DirectoryManager::forceInsertFileEntry(const QString &filePath) {
    if(!this->isFile(filePath) || containsFile(filePath))
        return false;
    QString fileName = QFileInfo(filePath).fileName();
    insertFileEntrySorted(statFileEntry(filePath, fileName));
    if(!directoryPath().isEmpty()) {
        qCDebug(logDirManager) << "fileIns" << filePath << directoryPath();
        emit fileAdded(filePath);
    }
    return true;
}

void DirectoryManager::removeFileEntry(const QString &filePath) {
    int index = indexOfFile(filePath);
    if(index < 0)
        return;
    fileEntryVec.erase(fileEntryVec.begin() + index);
    fileIndexCacheDirty = true;
    qCDebug(logDirManager) << "fileRem" << filePath;
    emit fileRemoved(filePath, index);
}

void DirectoryManager::updateFileEntry(const QString &filePath) {
    int index = indexOfFile(filePath);
    if(index < 0)
        return;
    FSEntry newEntry(filePath);
    if(fileEntryVec.at(index).modifyTime != newEntry.modifyTime)
        fileEntryVec.at(index) = newEntry;
    qCDebug(logDirManager) << "fileMod" << filePath;
    emit fileModified(filePath);
}

void DirectoryManager::renameFileEntry(const QString &oldFilePath, const QString &newFileName) {
    QFileInfo fi(oldFilePath);
    QString newFilePath = fi.absolutePath() + "/" + newFileName;
    if(!containsFile(oldFilePath)) {
        if(containsFile(newFilePath))
            updateFileEntry(newFilePath);
        else
            insertFileEntry(newFilePath);
        return;
    }
    if(!isSupportedFile(newFilePath)) {
        removeFileEntry(oldFilePath);
        return;
    }
    int replaceIndex = indexOfFile(newFilePath);
    if(replaceIndex >= 0) {
        fileEntryVec.erase(fileEntryVec.begin() + replaceIndex);
        fileIndexCacheDirty = true;
        emit fileRemoved(newFilePath, replaceIndex);
    }
    // remove the old one
    int oldIndex = indexOfFile(oldFilePath);
    fileEntryVec.erase(fileEntryVec.begin() + oldIndex);
    // insert
    insertFileEntrySorted(statFileEntry(newFilePath, newFileName));
    qCDebug(logDirManager) << "fileRen" << oldFilePath << newFilePath;
    emit fileRenamed(oldFilePath, oldIndex, newFilePath, indexOfFile(newFilePath));
}

// ---- dir entries

bool DirectoryManager::insertDirEntry(const QString &dirPath) {
    if(containsDir(dirPath))
        return false;
    QString dirName = QFileInfo(dirPath).fileName();
    FSEntry FSEntry;
    FSEntry.name = dirName;
    FSEntry.path = dirPath;
    FSEntry.isDirectory = true;
    insert_sorted(dirEntryVec, FSEntry, compareFunction());
    dirIndexCacheDirty = true;
    qCDebug(logDirManager) << "dirIns" << dirPath;
    emit dirAdded(dirPath);
    return true;
}

void DirectoryManager::removeDirEntry(const QString &dirPath) {
    int index = indexOfDir(dirPath);
    if(index < 0)
        return;
    dirEntryVec.erase(dirEntryVec.begin() + index);
    dirIndexCacheDirty = true;
    qCDebug(logDirManager) << "dirRem" << dirPath;
    emit dirRemoved(dirPath, index);
}

void DirectoryManager::renameDirEntry(const QString &oldDirPath, const QString &newDirName) {
    int oldIndex = indexOfDir(oldDirPath);
    if(oldIndex < 0)
        return;
    QFileInfo fi(oldDirPath);
    QString newDirPath = fi.absolutePath() + "/" + newDirName;
    // remove the old one
    dirEntryVec.erase(dirEntryVec.begin() + oldIndex);
    // insert
    FSEntry FSEntry;
    FSEntry.name = newDirName;
    FSEntry.path = newDirPath;
    FSEntry.isDirectory = true;
    insert_sorted(dirEntryVec, FSEntry, compareFunction());
    dirIndexCacheDirty = true;
    qCDebug(logDirManager) << "dirRen" << oldDirPath << newDirPath;
    emit dirRenamed(oldDirPath, oldIndex, newDirPath, indexOfDir(newDirPath));
}


FileListSource DirectoryManager::source() const {
    return mListSource;
}

QStringList DirectoryManager::fileList() const {
    QStringList list;
    for(auto const& value : fileEntryVec)
        list << value.path;
    return list;
}

bool DirectoryManager::fileWatcherActive() {
    if(!watcher)
        return false;
    return watcher->isObserving();
}

//----------------------------------------------------------------------------
// fs watcher events  ( onFile___External() )
// these take file NAMES, not paths
void DirectoryManager::onFileRemovedExternal(const QString& fileName) {
    QString fullPath = watcher->watchPath() + "/" + fileName;
    removeDirEntry(fullPath);
    removeFileEntry(fullPath);
}

void DirectoryManager::onFileAddedExternal(const QString& fileName) {
    QString fullPath = watcher->watchPath() + "/" + fileName;
    if(isDir(fullPath))
        insertDirEntry(fullPath);
    else
        insertFileEntry(fullPath);
}

void DirectoryManager::onFileRenamedExternal(const QString& oldName, const QString& newName) {
    QString oldPath = watcher->watchPath() + "/" + oldName;
    QString newPath = watcher->watchPath() + "/" + newName;
    if(isDir(newPath))
        renameDirEntry(oldPath, newName);
    else
        renameFileEntry(oldPath, newName);
}

void DirectoryManager::onFileModifiedExternal(const QString& fileName) {
    updateFileEntry(watcher->watchPath() + "/" + fileName);
}

#include "duplicateresultsmodel.h"

#include <QDateTime>
#include <QFileInfo>
#include <QLocale>
#include <QSet>

DuplicateResultsModel::DuplicateResultsModel(QObject *parent) : QAbstractItemModel(parent) {
}

void DuplicateResultsModel::clear() {
    beginResetModel();
    mGroups.clear();
    mGroupIndexByPath.clear();
    endResetModel();
    emit checkedCountChanged(0);
}

void DuplicateResultsModel::addMatch(const DuplicateMatch &match) {
    int groupRow = mGroupIndexByPath.value(match.sourcePath, -1);
    if(groupRow < 0) {
        groupRow = mGroups.count();
        beginInsertRows(QModelIndex(), groupRow, groupRow);
        Group group;
        group.sourcePath = match.sourcePath;
        group.dimensions = match.sourceDimensions;
        group.fileSize = match.sourceFileSize;
        mGroups.append(group);
        mGroupIndexByPath.insert(match.sourcePath, groupRow);
        endInsertRows();
    }
    Group &group = mGroups[groupRow];
    int childRow = group.rows.count();
    beginInsertRows(index(groupRow, 0), childRow, childRow);
    MatchRow row;
    row.match = match;
    group.rows.append(row);
    endInsertRows();
}

void DuplicateResultsModel::setThumbnail(const QString &path, const QPixmap &pixmap) {
    for(int g = 0; g < mGroups.count(); g++) {
        Group &group = mGroups[g];
        if(group.sourcePath == path) {
            group.thumb = pixmap;
            QModelIndex idx = index(g, COL_THUMB);
            emit dataChanged(idx, idx, {Qt::DecorationRole});
        }
        for(int r = 0; r < group.rows.count(); r++) {
            if(group.rows[r].match.matchPath == path) {
                group.rows[r].thumb = pixmap;
                QModelIndex idx = index(r, COL_THUMB, index(g, 0));
                emit dataChanged(idx, idx, {Qt::DecorationRole});
            }
        }
    }
}

QStringList DuplicateResultsModel::checkedPaths() const {
    QStringList paths;
    for(const Group &group : mGroups)
        for(const MatchRow &row : group.rows)
            if(row.checked == Qt::Checked)
                paths << row.match.matchPath;
    return paths;
}

QStringList DuplicateResultsModel::allMatchPaths() const {
    QStringList paths;
    for(const Group &group : mGroups)
        for(const MatchRow &row : group.rows)
            paths << row.match.matchPath;
    return paths;
}

void DuplicateResultsModel::smartSelect(SmartSelectMode mode) {
    for(int g = 0; g < mGroups.count(); g++) {
        Group &group = mGroups[g];
        if(mode == SELECT_ALL || mode == CLEAR_SELECTION) {
            for(MatchRow &row : group.rows)
                row.checked = (mode == SELECT_ALL) ? Qt::Checked : Qt::Unchecked;
        } else {
            // metric per candidate; index -1 is the group's source image
            auto metric = [&](int i) -> qint64 {
                QString path = (i < 0) ? group.sourcePath : group.rows[i].match.matchPath;
                switch(mode) {
                case KEEP_LARGEST_RESOLUTION: {
                    QSize d = (i < 0) ? group.dimensions : group.rows[i].match.matchDimensions;
                    return qint64(d.width()) * d.height();
                }
                case KEEP_LARGEST_FILE:
                    return (i < 0) ? group.fileSize : group.rows[i].match.matchFileSize;
                case KEEP_NEWEST:
                    return QFileInfo(path).lastModified().toSecsSinceEpoch();
                case KEEP_OLDEST:
                    return -QFileInfo(path).lastModified().toSecsSinceEpoch();
                default:
                    return 0;
                }
            };
            int best = -1;
            qint64 bestValue = metric(-1);
            for(int i = 0; i < group.rows.count(); i++) {
                if(metric(i) > bestValue) {
                    bestValue = metric(i);
                    best = i;
                }
            }
            for(int i = 0; i < group.rows.count(); i++)
                group.rows[i].checked = (i == best) ? Qt::Unchecked : Qt::Checked;
        }
        QModelIndex groupIdx = index(g, COL_CHECK);
        emit dataChanged(groupIdx, groupIdx, {Qt::CheckStateRole});
        if(!group.rows.isEmpty())
            emit dataChanged(index(0, COL_CHECK, index(g, 0)),
                             index(group.rows.count() - 1, COL_CHECK, index(g, 0)),
                             {Qt::CheckStateRole});
    }
    emitCheckedCount();
}

void DuplicateResultsModel::removeMatchesForPaths(const QStringList &paths) {
    QSet<QString> removed(paths.begin(), paths.end());
    beginResetModel();
    QVector<Group> kept;
    for(Group &group : mGroups) {
        if(removed.contains(group.sourcePath))
            continue;
        QVector<MatchRow> rows;
        for(MatchRow &row : group.rows)
            if(!removed.contains(row.match.matchPath))
                rows.append(row);
        if(rows.isEmpty())
            continue;
        group.rows = rows;
        kept.append(group);
    }
    mGroups = kept;
    mGroupIndexByPath.clear();
    for(int g = 0; g < mGroups.count(); g++)
        mGroupIndexByPath.insert(mGroups[g].sourcePath, g);
    endResetModel();
    emitCheckedCount();
}

qint64 DuplicateResultsModel::checkedBytes() const {
    qint64 total = 0;
    for(const Group &group : mGroups)
        for(const MatchRow &row : group.rows)
            if(row.checked == Qt::Checked)
                total += row.match.matchFileSize;
    return total;
}

QModelIndex DuplicateResultsModel::groupIndex(const QString &sourcePath) const {
    int row = mGroupIndexByPath.value(sourcePath, -1);
    return (row < 0) ? QModelIndex() : index(row, 0);
}

int DuplicateResultsModel::matchCount() const {
    int count = 0;
    for(const Group &group : mGroups)
        count += group.rows.count();
    return count;
}

//------------------------------------------------------------------------------

QModelIndex DuplicateResultsModel::index(int row, int column, const QModelIndex &parent) const {
    if(!hasIndex(row, column, parent))
        return QModelIndex();
    if(!parent.isValid())
        return createIndex(row, column, quintptr(0));
    return createIndex(row, column, quintptr(parent.row() + 1));
}

QModelIndex DuplicateResultsModel::parent(const QModelIndex &child) const {
    if(!child.isValid() || child.internalId() == 0)
        return QModelIndex();
    return createIndex(int(child.internalId()) - 1, 0, quintptr(0));
}

int DuplicateResultsModel::rowCount(const QModelIndex &parent) const {
    if(!parent.isValid())
        return mGroups.count();
    if(parent.internalId() == 0 && parent.column() == 0)
        return mGroups.at(parent.row()).rows.count();
    return 0;
}

int DuplicateResultsModel::columnCount(const QModelIndex &) const {
    return COLUMN_COUNT;
}

bool DuplicateResultsModel::isGroupIndex(const QModelIndex &index) const {
    return index.isValid() && index.internalId() == 0;
}

QVariant DuplicateResultsModel::data(const QModelIndex &index, int role) const {
    if(!index.isValid())
        return QVariant();
    if(isGroupIndex(index))
        return groupData(mGroups.at(index.row()), index.column(), role);
    const Group &group = mGroups.at(int(index.internalId()) - 1);
    return matchData(group.rows.at(index.row()), index.column(), role);
}

QVariant DuplicateResultsModel::groupData(const Group &group, int column, int role) const {
    if(role == IsGroupRole)
        return true;
    if(role == FilePathRole)
        return group.sourcePath;
    qreal maxSim = 0;
    for(const MatchRow &row : group.rows)
        maxSim = qMax(maxSim, row.match.similarity);
    if(role == SimilarityRole)
        return maxSim;
    if(role == Qt::CheckStateRole && column == COL_CHECK) {
        int checked = 0;
        for(const MatchRow &row : group.rows)
            if(row.checked == Qt::Checked)
                checked++;
        if(checked == 0)
            return Qt::Unchecked;
        return (checked == group.rows.count()) ? Qt::Checked : Qt::PartiallyChecked;
    }
    if(role == Qt::DecorationRole && column == COL_THUMB)
        return group.thumb;
    if(role == Qt::DisplayRole || role == SortRole) {
        switch(column) {
        case COL_SIMILARITY:
            return (role == SortRole) ? QVariant(maxSim)
                                      : QVariant(tr("%n match(es)", nullptr, group.rows.count()));
        case COL_DIMENSIONS:
            if(role == SortRole)
                return qint64(group.dimensions.width()) * group.dimensions.height();
            return QString("%1 x %2").arg(group.dimensions.width()).arg(group.dimensions.height());
        case COL_SIZE:
            return (role == SortRole) ? QVariant(group.fileSize)
                                      : QVariant(QLocale().formattedDataSize(group.fileSize, 1));
        case COL_NAME:
            return QFileInfo(group.sourcePath).fileName();
        case COL_PATH:
            return group.sourcePath;
        }
    }
    return QVariant();
}

QVariant DuplicateResultsModel::matchData(const MatchRow &row, int column, int role) const {
    if(role == IsGroupRole)
        return false;
    if(role == FilePathRole)
        return row.match.matchPath;
    if(role == SimilarityRole)
        return row.match.similarity;
    if(role == Qt::CheckStateRole && column == COL_CHECK)
        return row.checked;
    if(role == Qt::DecorationRole && column == COL_THUMB)
        return row.thumb;
    if(role == Qt::DisplayRole || role == SortRole) {
        switch(column) {
        case COL_SIMILARITY:
            if(role == SortRole)
                return row.match.similarity;
            return row.match.exact ? tr("100% (exact)")
                                   : QString("%1%").arg(row.match.similarity, 0, 'f', 1);
        case COL_DIMENSIONS:
            if(role == SortRole)
                return qint64(row.match.matchDimensions.width()) * row.match.matchDimensions.height();
            return QString("%1 x %2").arg(row.match.matchDimensions.width())
                                     .arg(row.match.matchDimensions.height());
        case COL_SIZE:
            return (role == SortRole) ? QVariant(row.match.matchFileSize)
                                      : QVariant(QLocale().formattedDataSize(row.match.matchFileSize, 1));
        case COL_NAME:
            return QFileInfo(row.match.matchPath).fileName();
        case COL_PATH:
            return row.match.matchPath;
        }
    }
    return QVariant();
}

bool DuplicateResultsModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if(!index.isValid() || role != Qt::CheckStateRole || index.column() != COL_CHECK)
        return false;
    auto state = static_cast<Qt::CheckState>(value.toInt());
    if(isGroupIndex(index)) {
        // checking a group checks/unchecks all its matches
        Group &group = mGroups[index.row()];
        Qt::CheckState target = (state == Qt::Unchecked) ? Qt::Unchecked : Qt::Checked;
        for(MatchRow &row : group.rows)
            row.checked = target;
        emit dataChanged(this->index(0, COL_CHECK, index),
                         this->index(group.rows.count() - 1, COL_CHECK, index), {Qt::CheckStateRole});
    } else {
        Group &group = mGroups[int(index.internalId()) - 1];
        group.rows[index.row()].checked = (state == Qt::Unchecked) ? Qt::Unchecked : Qt::Checked;
    }
    emit dataChanged(index, index, {Qt::CheckStateRole});
    QModelIndex groupIdx = isGroupIndex(index) ? index : index.parent();
    QModelIndex groupCheck = this->index(groupIdx.row(), COL_CHECK);
    emit dataChanged(groupCheck, groupCheck, {Qt::CheckStateRole});
    emitCheckedCount();
    return true;
}

Qt::ItemFlags DuplicateResultsModel::flags(const QModelIndex &index) const {
    Qt::ItemFlags f = QAbstractItemModel::flags(index);
    if(index.column() == COL_CHECK)
        f |= Qt::ItemIsUserCheckable;
    return f;
}

QVariant DuplicateResultsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch(section) {
    case COL_CHECK:      return "";
    case COL_THUMB:      return "";
    case COL_SIMILARITY: return tr("Similarity");
    case COL_DIMENSIONS: return tr("Dimensions");
    case COL_SIZE:       return tr("File size");
    case COL_NAME:       return tr("Name");
    case COL_PATH:       return tr("Path");
    }
    return QVariant();
}

void DuplicateResultsModel::emitCheckedCount() {
    emit checkedCountChanged(checkedPaths().count());
}

//------------------------------------------------------------------------------

DuplicateResultsProxy::DuplicateResultsProxy(QObject *parent) : QSortFilterProxyModel(parent) {
    setSortRole(DuplicateResultsModel::SortRole);
    setRecursiveFilteringEnabled(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

void DuplicateResultsProxy::setTextFilter(const QString &text) {
    mTextFilter = text;
    invalidateFilter();
}

void DuplicateResultsProxy::setMinSimilarity(qreal percent) {
    mMinSimilarity = percent;
    invalidateFilter();
}

bool DuplicateResultsProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    // group rows only appear when a child is accepted (recursive filtering)
    if(idx.data(DuplicateResultsModel::IsGroupRole).toBool())
        return false;
    if(idx.data(DuplicateResultsModel::SimilarityRole).toReal() < mMinSimilarity)
        return false;
    if(mTextFilter.isEmpty())
        return true;
    return idx.data(DuplicateResultsModel::FilePathRole).toString()
              .contains(mTextFilter, Qt::CaseInsensitive);
}

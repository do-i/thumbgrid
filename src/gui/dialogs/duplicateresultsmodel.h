#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QPixmap>
#include <QSortFilterProxyModel>
#include <QVector>
#include <cstdint>
#include "components/duplicatefinder/duplicatematch.h"

// Two-level tree of duplicate search results: parent rows are reference
// (source) images, children are their matches (docs/003 §3.3). With a
// single group it reads as a flat table. Every row's checkbox marks that
// row's own file, so a reference can be removed instead of its matches.
class DuplicateResultsModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Columns : std::uint8_t {
        COL_CHECK, COL_THUMB, COL_SIMILARITY, COL_DIMENSIONS,
        COL_SIZE, COL_NAME, COL_PATH, COLUMN_COUNT
    };
    enum Roles : std::uint16_t {
        SortRole = Qt::UserRole,
        FilePathRole,
        IsGroupRole,
        SimilarityRole
    };

    explicit DuplicateResultsModel(QObject *parent = nullptr);

    enum SmartSelectMode : std::uint8_t {
        SELECT_ALL, SELECT_ALL_REFERENCES, KEEP_LARGEST_RESOLUTION, KEEP_LARGEST_FILE,
        KEEP_NEWEST, KEEP_OLDEST, CLEAR_SELECTION
    };

    void clear();
    void addMatch(const DuplicateMatch &match);
    void setThumbnail(const QString &path, const QPixmap &pixmap);
    // SELECT_ALL checks every match, SELECT_ALL_REFERENCES every reference;
    // the KEEP_* modes check all matches per group except the "best" one by
    // the given criterion, and the group's source image also competes for
    // "best". Every mode leaves at least one copy per group unchecked.
    void smartSelect(SmartSelectMode mode);
    void removeMatchesForPaths(const QStringList &paths);
    QStringList checkedPaths() const;
    // groups whose reference and every match are all checked
    int groupsWithEveryCopyChecked() const;
    int matchCount() const;
    QModelIndex groupIndex(const QString &sourcePath) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

signals:
    void checkedCountChanged(int count);

private:
    struct MatchRow {
        DuplicateMatch match;
        Qt::CheckState checked = Qt::Unchecked;
        QPixmap thumb;
    };
    struct Group {
        QString sourcePath;
        QSize dimensions;
        qint64 fileSize = 0;
        Qt::CheckState checked = Qt::Unchecked;
        QPixmap thumb;
        QVector<MatchRow> rows;
    };

    bool isGroupIndex(const QModelIndex &index) const;
    QVariant groupData(const Group &group, int column, int role) const;
    QVariant matchData(const MatchRow &row, int column, int role) const;
    void emitCheckedCount();

    QVector<Group> mGroups;
    QHash<QString, int> mGroupIndexByPath;
};

class DuplicateResultsProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit DuplicateResultsProxy(QObject *parent = nullptr);
    void setTextFilter(const QString &text);
    void setMinSimilarity(qreal percent);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString mTextFilter;
    qreal mMinSimilarity = 0;
};

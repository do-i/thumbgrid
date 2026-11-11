#ifndef BOOKMARKSWIDGET_H
#define BOOKMARKSWIDGET_H

#include <QWidget>
#include "gui/folderview/bookmarksitem.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QMimeData>
#include "settings.h"

class BookmarksWidget : public QWidget {
    Q_OBJECT

public:
    explicit BookmarksWidget(QWidget *parent = nullptr);
    ~BookmarksWidget() override;

public slots:
    void addBookmark(const QString& directoryPath);

    void removeBookmark(const QString& dirPath);
    bool hasBookmark(const QString& dirPath) const;
    void onPathChanged(const QString& path);
private slots:
    void readSettings();

    void saveBookmarks();
signals:
    // the list gained, lost or reordered rows
    void bookmarksChanged();
    void bookmarkClicked(const QString& dirPath);
    void droppedIn(const QStringList& paths, const QString& dirPath, bool copy);
    // dirPath is empty when the pointer was over the panel and not over a row
    void contextMenuRequested(const QString& dirPath, const QPoint& globalPos);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
private:
    static QString labelFor(const QString& dirPath);
    BookmarksItem *itemFor(const QString& dirPath) const;
    BookmarksItem *createItem(const QString& dirPath);
    void dropItem(BookmarksItem *item);

    QVBoxLayout layout;
    QStringList paths;
    QString highlightedPath;
};

#endif // BOOKMARKSWIDGET_H

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
    void onPathChanged(const QString& path);
private slots:
    void readSettings();

    void saveBookmarks();
signals:
    void bookmarkClicked(const QString& dirPath);
    void droppedIn(const QStringList& paths, const QString& dirPath);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
private:
    QVBoxLayout layout;
    QStringList paths;
    QString highlightedPath;
};

#endif // BOOKMARKSWIDGET_H

#ifndef FILESYSTEMMODELCUSTOM_H
#define FILESYSTEMMODELCUSTOM_H

#include <QApplication>
#include <QFileSystemModel>
#include <QPainter>
#include "settings.h"
#include "utils/imagelib.h"

class FileSystemModelCustom : public QFileSystemModel
{
    Q_OBJECT
public:
    FileSystemModelCustom(QObject *parent = nullptr);
    QVariant data(const QModelIndex &index, int role) const override;

signals:
    // the folder icon was re-tinted; every visible row's decoration changed
    void folderIconChanged();

protected:
    Qt::ItemFlags flags(const QModelIndex &index) const override;
private:
    void readSettings();

    QPixmap folderIcon;
    QColor mIconColor;
};

#endif // FILESYSTEMMODELCUSTOM_H

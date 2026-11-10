#include "filesystemmodelcustom.h"

#include <QAbstractFileIconProvider>

namespace {
// data() replaces every icon, so the model need not resolve system icons
class NoIconProvider : public QAbstractFileIconProvider {
public:
    QIcon icon(IconType) const override { return {}; }
    QIcon icon(const QFileInfo &) const override { return {}; }
};
}

FileSystemModelCustom::FileSystemModelCustom(QObject *parent) : QFileSystemModel(parent) {
    static NoIconProvider iconProvider; // outlives every model and its gatherer thread
    setIconProvider(&iconProvider);
    qreal dpr = qApp->devicePixelRatio();
    QString iconPath = ":/res/icons/common/menuitem/folder16.png";
    if(dpr >= (1.0 + 0.001))
        iconPath.replace(".", "@2x.");
    folderIcon.load(iconPath);
    readSettings();

    connect(settings, &Settings::settingsChanged, this, &FileSystemModelCustom::readSettings);
}

void FileSystemModelCustom::readSettings() {
    const QColor color = settings->colorScheme().icons;
    if(color == mIconColor)
        return;
    const bool initial = !mIconColor.isValid();
    mIconColor = color;
    ImageLib::recolor(this->folderIcon, color);
    if(!initial)
        emit folderIconChanged();
}

QVariant FileSystemModelCustom::data( const QModelIndex& index, int role ) const {
    if(role == Qt::DecorationRole)
        return folderIcon;
    return QFileSystemModel::data(index, role);
}

Qt::ItemFlags FileSystemModelCustom::flags(const QModelIndex& index) const {
    if(!index.isValid()) {
        return Qt::NoItemFlags; // 0
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}

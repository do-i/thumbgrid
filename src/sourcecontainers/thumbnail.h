#pragma once

#include <QString>
#include <QPixmap>
#include <memory>

class Thumbnail {
public:
    Thumbnail(QString _name, QString _info, int _size, const std::shared_ptr<QPixmap>& _pixmap, bool _transparencyGridEligible = true);
    QString name();
    QString info();
    int size();
    bool hasAlphaChannel();
    bool transparencyGridEligible();
    // whether the source entry is a symbolic link; drawn as a corner badge in the grid
    bool isSymlink() const;
    void setSymlink(bool mode);
    std::shared_ptr<QPixmap> pixmap();
private:
    QString mName, mInfo;
    std::shared_ptr<QPixmap> mPixmap;
    int mSize;
    bool mHasAlphaChannel, mTransparencyGridEligible;
    bool mIsSymlink = false;
};

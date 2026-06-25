#include "thumbnail.h"

Thumbnail::Thumbnail(QString _name, QString _info, int _size, std::shared_ptr<QPixmap> _pixmap, bool _transparencyGridEligible)
    : mName(_name),
      mInfo(_info),
      mPixmap(_pixmap),
      mSize(_size),
      mHasAlphaChannel(false),
      mTransparencyGridEligible(_transparencyGridEligible)
{
    if(_pixmap)
        mHasAlphaChannel = _pixmap->hasAlphaChannel();
}

QString Thumbnail::name() {
    return mName;
}

QString Thumbnail::info() {
    return mInfo;
}

int Thumbnail::size() {
    return mSize;
}

bool Thumbnail::hasAlphaChannel() {
    return mHasAlphaChannel;
}

bool Thumbnail::transparencyGridEligible() {
    return mTransparencyGridEligible;
}

std::shared_ptr<QPixmap> Thumbnail::pixmap() {
    return mPixmap;
}

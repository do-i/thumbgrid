#include "sharedresources.h"
#include "utils/imagelib.h"

SharedResources *shrRes = nullptr;

SharedResources::SharedResources()
{
}

SharedResources::~SharedResources() {
    delete shrRes;
}

QPixmap *SharedResources::getPixmap(ShrIcon icon, qreal dpr) {
    QPixmap *pixmap;
    QString path;
    if(icon == ShrIcon::SHR_ICON_ERROR) {
        path = ":/res/icons/common/other/loading-error72.png";
        pixmap = mLoadingErrorIcon72;
    } else {
        path = ":/res/icons/common/other/loading72.png";
        pixmap = mLoadingIcon72;
    }
    if(pixmap)
        return pixmap;

    qreal pixmapDrawScale;
    if(dpr >= (1.0 + 0.001)) {
        path.replace(".", "@2x.");
        pixmap = new QPixmap(path);
        if(dpr >= (2.0 - 0.001))
            pixmapDrawScale = dpr;
        else
            pixmapDrawScale = 2.0;
        pixmap->setDevicePixelRatio(pixmapDrawScale);
    } else {
        pixmap = new QPixmap(path);
    }
    if(icon == ShrIcon::SHR_ICON_ERROR)
        mLoadingErrorIcon72 = pixmap;
    else
        mLoadingIcon72 = pixmap;
    return pixmap;
}

// getPixmap() fixes the icon's resolution on first use, so icon and color give the whole key.
const QPixmap &SharedResources::getTintedPixmap(ShrIcon icon, qreal dpr, const QColor &color) {
    const quint64 key = (static_cast<quint64>(color.rgba()) << 8) | static_cast<quint64>(icon);
    auto it = mTinted.constFind(key);
    if(it != mTinted.constEnd())
        return *it;
    if(mTinted.size() >= 8) // colors only change with the theme
        mTinted.clear();
    QPixmap tinted(*getPixmap(icon, dpr));
    ImageLib::recolor(tinted, color);
    return *mTinted.insert(key, tinted);
}

SharedResources *SharedResources::getInstance() {
    if(!shrRes) {
        shrRes = new SharedResources();
    }
    return shrRes;
}

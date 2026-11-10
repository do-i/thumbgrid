#pragma once

#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QDebug>
#include <cstdint>

enum ShrIcon : std::uint8_t {
    SHR_ICON_ERROR,
    SHR_ICON_LOADING
};

class SharedResources
{
public:
    SharedResources();
    static SharedResources* getInstance();
    ~SharedResources();

    QPixmap *getPixmap(ShrIcon icon, qreal dpr);
    // the icon recolored to `color`, built once per icon and color and shared by every widget
    const QPixmap &getTintedPixmap(ShrIcon icon, qreal dpr, const QColor &color);
private:
    QHash<quint64, QPixmap> mTinted;
    QPixmap *mLoadingIcon72 = nullptr;
    QPixmap *mLoadingErrorIcon72 = nullptr;
};

extern SharedResources *shrRes;

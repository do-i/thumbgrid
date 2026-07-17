#include "thumbnailwidget.h"

ThumbnailWidget::ThumbnailWidget(QGraphicsItem *parent) :
    QGraphicsWidget(parent),
    isLoaded(false),
    thumbnail(nullptr),
    highlighted(false),
    hovered(false),
    dropHovered(false),
    mShowInfo(true),
    mFixedBackgroundRect(false),
    mCellBorderVisible(false),
    mTransparencyGridVisible(false),
    mThumbnailSize(100),
    mThumbnailWidth(100),
    mThumbnailHeight(100),
    mThumbnailTopMargin(0),
    padding(5),
    marginX(2),
    marginY(2),
    labelSpacing(9),
    textHeight(5),
    mLabelBackgroundColor(settings->folderViewLabelBackgroundColor()),
    thumbStyle(THUMB_SIMPLE)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    dpr = qApp->devicePixelRatio();
    if(trunc(dpr) == dpr) // don't enable for fractional scaling
        setCacheMode(QGraphicsItem::DeviceCoordinateCache);
    setAcceptHoverEvents(true);
    font.setBold(false);
    font.setPointSize(settings->folderViewFontPointSize());
    QFontMetrics fm(font);
    textHeight = fm.height();
}

void ThumbnailWidget::updateDpr(qreal newDpr) {
    if(dpr != newDpr) {
        dpr = newDpr;
        if(trunc(dpr) == dpr) // don't enable for fractional scaling
            setCacheMode(QGraphicsItem::DeviceCoordinateCache);
        updateThumbnailDrawPosition();
        updateBackgroundRect();
    }
}

void ThumbnailWidget::setThumbnailSize(int size) {
    if(mThumbnailSize != size && size > 0) {
        isLoaded = false;
        mThumbnailSize = size;
        mThumbnailWidth = size;
        mThumbnailHeight = size;
        updateBoundingRect();
        updateGeometry();
        updateThumbnailDrawPosition();
        updateBackgroundRect();
        setupTextLayout();
        update();
    }
}

void ThumbnailWidget::setThumbnailAreaSize(int width, int height) {
    if(width <= 0 || height <= 0)
        return;
    if(mThumbnailWidth != width || mThumbnailHeight != height) {
        mThumbnailWidth = width;
        mThumbnailHeight = height;
        updateThumbnailHeightForCellRatio();
        updateBoundingRect();
        updateGeometry();
        updateThumbnailDrawPosition();
        updateBackgroundRect();
        setupTextLayout();
        update();
    }
}

void ThumbnailWidget::setPadding(int _padding) {
    padding = _padding;
    updateBoundingRect();
}

void ThumbnailWidget::setMargins(int _marginX, int _marginY) {
    marginX = _marginX;
    marginY = _marginY;
    updateBoundingRect();
}

void ThumbnailWidget::setLabelSpacing(int _labelSpacing) {
    if(labelSpacing != _labelSpacing) {
        labelSpacing = _labelSpacing;
        updateBoundingRect();
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        updateGeometry();
        update();
    }
}

void ThumbnailWidget::setFixedBackgroundRect(bool mode) {
    if(mFixedBackgroundRect != mode) {
        mFixedBackgroundRect = mode;
        updateBackgroundRect();
        update();
    }
}

void ThumbnailWidget::setCellBorderVisible(bool mode) {
    if(mCellBorderVisible != mode) {
        mCellBorderVisible = mode;
        update();
    }
}

void ThumbnailWidget::setTransparencyGridVisible(bool mode) {
    if(mTransparencyGridVisible != mode) {
        mTransparencyGridVisible = mode;
        update();
    }
}

void ThumbnailWidget::setThumbnailTopMargin(int margin) {
    margin = qMax(0, margin);
    if(mThumbnailTopMargin != margin) {
        mThumbnailTopMargin = margin;
        updateThumbnailDrawPosition();
        updateBackgroundRect();
        update();
    }
}

void ThumbnailWidget::setCellHeightRatio(qreal heightOverWidth) {
    if(heightOverWidth < 0.0)
        heightOverWidth = 0.0;
    if(!qFuzzyCompare(mCellHeightRatio, heightOverWidth)) {
        mCellHeightRatio = heightOverWidth;
        updateThumbnailHeightForCellRatio();
        updateBoundingRect();
        updateGeometry();
        updateThumbnailDrawPosition();
        updateBackgroundRect();
        setupTextLayout();
        update();
    }
}

int ThumbnailWidget::thumbnailSize() {
    return mThumbnailSize;
}

void ThumbnailWidget::reset() {
    if(thumbnail)
        thumbnail.reset();
    highlighted = false;
    hovered = false;
    isLoaded = false;
    update();
}

void ThumbnailWidget::setThumbStyle(ThumbnailStyle _style) {
    if(thumbStyle != _style) {
        thumbStyle = _style;
        updateBoundingRect();
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        updateGeometry();
        update();
    }
}

void ThumbnailWidget::setShowInfo(bool mode) {
    if(mShowInfo != mode) {
        mShowInfo = mode;
        updateBoundingRect();
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        updateGeometry();
        update();
    }
}

void ThumbnailWidget::setLabelFontPointSize(int size) {
    size = qBound(6, size, 48);
    if(font.pointSize() != size) {
        font.setPointSize(size);
        QFontMetrics fm(font);
        textHeight = fm.height();
        updateBoundingRect();
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        updateGeometry();
        update();
    }
}

void ThumbnailWidget::setLabelBackgroundColor(const QColor &color) {
    if(color.isValid() && mLabelBackgroundColor != color) {
        mLabelBackgroundColor = color;
        update();
    }
}

// An invalid color disables the cell background fill (lets the view background
// show through), which is the default for the thumbnail strip.
void ThumbnailWidget::setCellBackgroundColor(const QColor &color) {
    if(mCellBackgroundColor != color) {
        mCellBackgroundColor = color;
        update();
    }
}

void ThumbnailWidget::updateGeometry() {
    QGraphicsWidget::updateGeometry();
}

void ThumbnailWidget::setGeometry(const QRectF &rect) {
    QGraphicsWidget::setGeometry(QRectF(rect.topLeft(), boundingRect().size()));
}

QRectF ThumbnailWidget::geometry() const {
    return QRectF(QGraphicsWidget::geometry().topLeft(), boundingRect().size());
}

QSizeF ThumbnailWidget::effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    return sizeHint(which, constraint);
}

void ThumbnailWidget::setThumbnail(const std::shared_ptr<Thumbnail>& _thumbnail) {
    if(_thumbnail) {
        thumbnail = _thumbnail;
        isLoaded = true;
        updateThumbnailDrawPosition();
        setupTextLayout();
        updateBackgroundRect();
        update();
    }
}

void ThumbnailWidget::unsetThumbnail() {
    if(thumbnail)
        thumbnail.reset();
    isLoaded = false;
}

void ThumbnailWidget::setupTextLayout() {
    if(thumbStyle != THUMB_SIMPLE) {
        if(mCellBorderVisible) {
            QRectF labelRect = labelBackgroundRect();
            nameRect = QRect(qRound(labelRect.left()),
                             qRound(labelRect.top()),
                             qRound(labelRect.width()),
                             textHeight);
        } else {
            nameRect = QRect(padding + marginX,
                             padding + marginY + mThumbnailHeight + labelSpacing,
                             mThumbnailWidth, textHeight);
        }
        if(mShowInfo)
            infoRect = nameRect.adjusted(0, textHeight + 2, 0, textHeight + 2);
        else
            infoRect = QRect();
    }
}

void ThumbnailWidget::updateBackgroundRect() {
    if(mFixedBackgroundRect) {
        bgRect = boundingRect().adjusted(marginX, marginY, -marginX, -marginY);
        return;
    }

    bool verticalFit = (drawRectCentered.height() >= drawRectCentered.width());
    if(thumbStyle == THUMB_NORMAL && !verticalFit) {
        bgRect.setBottom(height() - marginY);
        bgRect.setLeft(marginX);
        bgRect.setRight(width() - marginX);
        if(!thumbnail || !thumbnail->pixmap())
            bgRect.setTop(drawRectCentered.top() - padding);
        else // ensure we get equal padding on the top & sides
            bgRect.setTop(qMax(drawRectCentered.top() - drawRectCentered.left() + marginX, marginY));
    } else {
        bgRect = boundingRect().adjusted(marginX, marginY, -marginX, -marginY);
    }
}

void ThumbnailWidget::setHighlighted(bool mode) {
    if(highlighted != mode) {
        highlighted = mode;
        update();
    }
}

bool ThumbnailWidget::isHighlighted() {
    return highlighted;
}

void ThumbnailWidget::setDropHovered(bool mode) {
    if(dropHovered != mode) {
        dropHovered = mode;
        update();
    }
}

bool ThumbnailWidget::isDropHovered() {
    return dropHovered;
}

QRectF ThumbnailWidget::boundingRect() const {
    return mBoundingRect;
}

void ThumbnailWidget::updateBoundingRect() {
    updateThumbnailHeightForCellRatio();
    mBoundingRect = QRectF(0, 0,
                           mThumbnailWidth + (padding + marginX) * 2,
                           mThumbnailHeight + (padding + marginY) * 2);
    if(thumbStyle != THUMB_SIMPLE)
        mBoundingRect.adjust(0, 0, 0, labelSpacing + textHeight * (mShowInfo ? 2 : 1));
}

qreal ThumbnailWidget::width() {
    return boundingRect().width();
}

qreal ThumbnailWidget::height() {
    return boundingRect().height();
}

void ThumbnailWidget::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    Q_UNUSED(widget)
    Q_UNUSED(option)
    painter->setRenderHints(QPainter::Antialiasing);

    // update dpr since on wayland we only get correct values on the first paint event
    // actual thumbnail still has incorrect dpr but it is still drawn "correctly" so whatever
    updateDpr(painter->paintEngine()->paintDevice()->devicePixelRatioF());

    if(mCellBackgroundColor.isValid())
        drawCellBackground(painter);
    if(isHovered() && !isHighlighted())
        drawHoverBg(painter);
    if(isHighlighted())
        drawHighlight(painter);

    if(!thumbnail) { // not loaded
        // todo: recolor once in shrRes
        QPixmap loadingIcon(*shrRes->getPixmap(ShrIcon::SHR_ICON_LOADING, dpr));
        if(isHighlighted())
            ImageLib::recolor(loadingIcon, settings->folderViewSelectionColor());
        else
            ImageLib::recolor(loadingIcon, settings->colorScheme().folderview_hc2);
        drawIcon(painter, &loadingIcon);
    } else {
        if(!thumbnail->pixmap() || thumbnail->pixmap().get()->width() == 0) { // invalid thumb
            QPixmap errorIcon(*shrRes->getPixmap(ShrIcon::SHR_ICON_ERROR, dpr));
            if(isHighlighted())
                ImageLib::recolor(errorIcon, settings->folderViewSelectionColor());
            else
                ImageLib::recolor(errorIcon, settings->colorScheme().folderview_hc2);
            drawIcon(painter, &errorIcon);
        } else {
            drawThumbnail(painter, thumbnail->pixmap().get());
            if(isHovered())
                drawHoverHighlight(painter);
        }
        if(thumbStyle != THUMB_SIMPLE)
            drawLabel(painter);
    }
    if(mCellBorderVisible)
        drawCellBorder(painter);
    if(isDropHovered())
        drawDropHover(painter);
    if(isHighlighted())
        drawHighlightBorder(painter);
}

void ThumbnailWidget::drawHighlight(QPainter *painter) {
    if(isHighlighted()) {
        auto hints = painter->renderHints();
        auto op = painter->opacity();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setOpacity(0.40f * op);
        painter->fillRect(bgRect, settings->folderViewSelectionColor());
        painter->setOpacity(op);
        painter->setRenderHints(hints);
    }
}

void ThumbnailWidget::drawHighlightBorder(QPainter *painter) {
    if(isHighlighted()) {
        auto hints = painter->renderHints();
        auto op = painter->opacity();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setOpacity(0.70f * op);
        QPen pen(settings->folderViewSelectionColor(), 2);
        painter->setPen(pen);
        painter->drawRect(bgRect.adjusted(1,1,-1,-1)); // 2px pen
        painter->setOpacity(op);
        painter->setRenderHints(hints);
    }
}

void ThumbnailWidget::drawCellBackground(QPainter *painter) {
    auto op = painter->opacity();
    painter->fillRect(bgRect, mCellBackgroundColor);
    painter->setOpacity(op);
}

void ThumbnailWidget::drawHoverBg(QPainter *painter) {
    auto op = painter->opacity();
    painter->fillRect(bgRect, settings->colorScheme().folderview_hc);
    painter->setOpacity(op);
}

void ThumbnailWidget::drawHoverHighlight(QPainter *painter) {
    auto op = painter->opacity();
    auto mode = painter->compositionMode();
    painter->setCompositionMode(QPainter::CompositionMode_Plus);
    painter->setOpacity(0.2f);
    painter->drawPixmap(drawRectCentered, *thumbnail->pixmap());
    painter->setOpacity(op);
    painter->setCompositionMode(mode);
}

void ThumbnailWidget::drawCellBorder(QPainter *painter) {
    auto hints = painter->renderHints();
    painter->setRenderHint(QPainter::Antialiasing, false);
    QPen pen(settings->colorScheme().widget_border, 1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(bgRect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter->setRenderHints(hints);
}

void ThumbnailWidget::drawTransparencyGrid(QPainter *painter) {
    ImageLib::drawTransparencyGrid(painter, drawRectCentered);
}

void ThumbnailWidget::drawLabel(QPainter *painter) {
    if(thumbnail) {
        QColor labelBg = isHighlighted() ? settings->folderViewSelectedLabelBackgroundColor()
                                         : mLabelBackgroundColor;
        if(mCellBorderVisible)
            painter->fillRect(labelBackgroundRect(), labelBg);
        else
            painter->fillRect(nameRect.adjusted(-4, -1, 4, 1), labelBg);
        drawSingleLineText(painter, nameRect, thumbnail->name(), settings->colorScheme().text_hc2);
        if(mShowInfo) {
            auto op = painter->opacity();
            painter->setOpacity(op * 0.62f);
            drawSingleLineText(painter, infoRect, thumbnail->info(), settings->colorScheme().text_hc2);
            painter->setOpacity(op);
        }
    }
}

QRectF ThumbnailWidget::labelBackgroundRect() const {
    if(!mCellBorderVisible)
        return QRectF(nameRect.adjusted(-4, -1, 4, 1));

    int blockHeight = labelBlockHeight();
    return QRectF(marginX,
                  mBoundingRect.height() - marginY - blockHeight,
                  mBoundingRect.width() - marginX * 2,
                  blockHeight);
}

int ThumbnailWidget::labelBlockHeight() const {
    return textHeight * (mShowInfo ? 2 : 1) + (mShowInfo ? 2 : 0);
}

void ThumbnailWidget::drawSingleLineText(QPainter *painter, QRect rect, const QString& text, const QColor &color) {
    QFontMetrics fm(font);
    bool fits = !(fm.horizontalAdvance(text) > rect.width());
    // filename
    int flags;
    painter->setFont(font);
    if(fits) {
        flags = Qt::TextSingleLine | Qt::AlignVCenter | Qt::AlignHCenter;
        painter->setPen(color);
        painter->drawText(rect, flags, text);
    } else {
        // fancy variant with text fade effect - uses temporary surface to paint; slow
        QPixmap textLayer(rect.width() * dpr, rect.height() * dpr);
        textLayer.fill(Qt::transparent);
        textLayer.setDevicePixelRatio(dpr);
        QPainter textPainter(&textLayer);
        textPainter.setFont(font);
        // paint text onto tmp layer
        flags = Qt::TextSingleLine | Qt::AlignVCenter;
        textPainter.setPen(color);
        QRect textRect = QRect(0, 0, rect.width(), rect.height());
        textPainter.drawText(textRect, flags, text);
        QRectF fadeRect = textRect.adjusted(textRect.width() - 6,0,0,0);
        // fade effect
        QLinearGradient gradient(fadeRect.topLeft(), fadeRect.topRight());
        gradient.setColorAt(0, Qt::transparent);
        gradient.setColorAt(1, Qt::red); // any color, this is just a transparency mask
        textPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        textPainter.fillRect(fadeRect, gradient);
        // write text layer into graphicsitem
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter->drawPixmap(rect.topLeft(), textLayer);
    }
}

void ThumbnailWidget::drawDropHover(QPainter *painter) {
    // save
    auto hints = painter->renderHints();
    auto op = painter->opacity();

    painter->setRenderHint(QPainter::Antialiasing);
    QColor clr(190,60,25);
    painter->setOpacity(0.1f * op);
    painter->fillRect(bgRect, clr);
    painter->setOpacity(op);
    QPen pen(clr, 2);
    painter->setPen(pen);
    painter->drawRect(bgRect.adjusted(1,1,-1,-1));
    painter->setRenderHints(hints);
}

void ThumbnailWidget::drawThumbnail(QPainter* painter, const QPixmap *pixmap) {
    drawThumbnailShadow(painter, pixmap);
    if(mTransparencyGridVisible && thumbnail->hasAlphaChannel() && thumbnail->transparencyGridEligible())
        drawTransparencyGrid(painter);
    painter->drawPixmap(drawRectCentered, *pixmap);
}

void ThumbnailWidget::drawThumbnailShadow(QPainter *painter, const QPixmap *pixmap) {
    auto op = painter->opacity();
    if(thumbnail->hasAlphaChannel()) {
        QPixmap shadow(*pixmap);
        QPainter shadowPainter(&shadow);
        shadowPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        shadowPainter.fillRect(shadow.rect(), QColor(0, 0, 0, 90));
        shadowPainter.end();

        painter->setOpacity(op * 0.32f);
        painter->drawPixmap(drawRectCentered.translated(2, 3), shadow);
        painter->setOpacity(op * 0.18f);
        painter->drawPixmap(drawRectCentered.translated(0, 1), shadow);
    } else {
        painter->setOpacity(op * 0.34f);
        painter->fillRect(drawRectCentered.translated(2, 3), QColor(0, 0, 0, 90));
        painter->setOpacity(op * 0.16f);
        painter->fillRect(drawRectCentered.translated(0, 1), QColor(0, 0, 0, 90));
    }
    painter->setOpacity(op);
}

void ThumbnailWidget::drawIcon(QPainter* painter, const QPixmap *pixmap) {
    QPointF drawPosCentered(width()  / 2 - pixmap->width()  / (2 * pixmap->devicePixelRatioF()),
                            height() / 2 - pixmap->height() / (2 * pixmap->devicePixelRatioF()));
    painter->drawPixmap(drawPosCentered, *pixmap, QRectF(QPoint(0,0), pixmap->size()));
}

QSizeF ThumbnailWidget::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    Q_UNUSED(which);
    Q_UNUSED(constraint);

    return boundingRect().size();
}

void ThumbnailWidget::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    event->ignore();
    setHovered(true);
}

void ThumbnailWidget::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    event->ignore();
    setHovered(false);
}

void ThumbnailWidget::setHovered(bool mode) {
    if(hovered != mode) {
        hovered = mode;
        update();
    }
}

bool ThumbnailWidget::isHovered() {
    return hovered;
}

void ThumbnailWidget::updateThumbnailDrawPosition() {
    if(thumbnail && thumbnail->pixmap()) {
        QPoint topLeft;
        QSize pixmapSize; // dpr-adjusted size
        if(isLoaded)
            pixmapSize = thumbnail->pixmap()->size() / qApp->devicePixelRatio();
        else
            pixmapSize = thumbnail->pixmap()->size().scaled(mThumbnailSize, mThumbnailSize, Qt::KeepAspectRatio);
        topLeft.setX((width()  - pixmapSize.width())  / 2.0);
        if(thumbStyle == THUMB_SIMPLE)
            topLeft.setY((height() - pixmapSize.height()) / 2.0);
        else if(thumbStyle == THUMB_NORMAL_CENTERED)
            topLeft.setY(padding + marginY + (mThumbnailHeight - pixmapSize.height()) / 2.0);
        else { // THUMB_NORMAL - snap thumbnail to the filename label
            int topAligned = padding + marginY + mThumbnailTopMargin;
            int bottomAligned = padding + marginY + mThumbnailHeight - pixmapSize.height();
            topLeft.setY(qMax(topAligned, bottomAligned));
        }
        drawRectCentered = QRect(topLeft, pixmapSize);
    }
}

void ThumbnailWidget::updateThumbnailHeightForCellRatio() {
    if(mCellHeightRatio <= 0.0)
        return;

    int cellWidth = mThumbnailWidth + (padding + marginX) * 2;
    int labelBlockHeight = 0;
    if(thumbStyle != THUMB_SIMPLE)
        labelBlockHeight = labelSpacing + textHeight * (mShowInfo ? 2 : 1);

    int targetCellHeight = qRound(cellWidth * mCellHeightRatio);
    int targetThumbnailHeight = targetCellHeight - (padding + marginY) * 2 - labelBlockHeight;
    mThumbnailHeight = qMax(1, targetThumbnailHeight);
}

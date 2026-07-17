#pragma once

#include <QGraphicsWidget>
#include <QGraphicsItem>
#include <QGraphicsLayoutItem>
#include <QMouseEvent>
#include <QPainter>
#include <QGraphicsSceneHoverEvent>
#include <QPaintEngine>
#include <cmath>
#include "sourcecontainers/thumbnail.h"
#include "utils/imagelib.h"
#include "settings.h"
#include "sharedresources.h"

enum ThumbnailStyle {
    THUMB_SIMPLE,
    THUMB_NORMAL,
    THUMB_NORMAL_CENTERED
};

class ThumbnailWidget : public QGraphicsWidget {
    Q_OBJECT

public:
    ThumbnailWidget(QGraphicsItem *parent = nullptr);

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    bool isLoaded;
    void setThumbnail(const std::shared_ptr<Thumbnail>& _thumbnail);

    void setHighlighted(bool mode);
    bool isHighlighted();

    void setDropHovered(bool mode);
    bool isDropHovered();

    QRectF boundingRect() const override;

    qreal width();
    qreal height();
    void setThumbnailSize(int size);

    void setGeometry(const QRectF &rect) override;

    virtual QRectF geometry() const;
    QSizeF effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    void setThumbStyle(ThumbnailStyle _style);
    void setShowInfo(bool mode);
    void setPadding(int _padding);
    void setMargins(int _marginX, int _marginY);
    void setLabelSpacing(int _labelSpacing);
    void setThumbnailAreaSize(int width, int height);
    void setFixedBackgroundRect(bool mode);
    void setCellBorderVisible(bool mode);
    void setTransparencyGridVisible(bool mode);
    void setThumbnailTopMargin(int margin);
    void setCellHeightRatio(qreal heightOverWidth);
    void setLabelFontPointSize(int size);
    void setLabelBackgroundColor(const QColor &color);
    void setCellBackgroundColor(const QColor &color);
    int thumbnailSize();
    void reset();
    void unsetThumbnail();

protected:
    void setupTextLayout();
    void drawThumbnail(QPainter* painter, const QPixmap *pixmap);
    void drawIcon(QPainter *painter, const QPixmap *pixmap);
    void drawHighlight(QPainter *painter);
    void drawHighlightBorder(QPainter *painter);
    void drawHoverBg(QPainter *painter);
    void drawHoverHighlight(QPainter *painter);
    void drawCellBorder(QPainter *painter);
    void drawTransparencyGrid(QPainter *painter);
    void drawLabel(QPainter *painter);
    QRectF labelBackgroundRect() const;
    int labelBlockHeight() const;
    void drawThumbnailShadow(QPainter *painter, const QPixmap *pixmap);
    void drawDropHover(QPainter *painter);
    void drawSingleLineText(QPainter *painter, QRect rect, const QString& text, const QColor &color);
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *item, QWidget *widget) override;
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    void updateGeometry() override;
    void setHovered(bool);
    bool isHovered();
    void updateBackgroundRect();
    void updateThumbnailDrawPosition();
    void updateThumbnailHeightForCellRatio();
    void updateDpr(qreal newDpr);

    std::shared_ptr<Thumbnail> thumbnail;
    bool highlighted, hovered, dropHovered, mShowInfo, mFixedBackgroundRect, mCellBorderVisible, mTransparencyGridVisible;
    int mThumbnailSize, mThumbnailWidth, mThumbnailHeight, mThumbnailTopMargin, padding, marginX, marginY, labelSpacing, textHeight;
    qreal mCellHeightRatio = 0.0;
    QRectF bgRect, mBoundingRect;
    QColor mLabelBackgroundColor;
    QColor mCellBackgroundColor;
    void drawCellBackground(QPainter *painter);
    QFont font, fontInfo;
    QRect drawRectCentered, nameRect, infoRect;
    qreal dpr = 1.0;
    void updateBoundingRect();
    ThumbnailStyle thumbStyle;
};

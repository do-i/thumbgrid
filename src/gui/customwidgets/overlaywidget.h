/* Base class for floating widgets.
 * It will automatically reposition itself according to FloatingWidgetPosition.
 *
 * Placement rule: status overlays sit in a corner or bottom-centre; only input overlays (rename) are centred.
 * Corner overlays (copy: left, save-confirm: right) flip from bottom to top while the
 * main panel is at the bottom, so they stay clear of the panel and video controls.
 * Messages (bottom) and the move progress (bottom-right) keep the footer clearance margin.
 */

#pragma once

#include "gui/customwidgets/floatingwidget.h"
#include <QTimeLine>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QDebug>
#include <cstdint>

enum FloatingWidgetPosition : std::uint8_t {
    LEFT,
    RIGHT,
    BOTTOM,
    TOP,
    TOPLEFT,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT,
    CENTER
};

class OverlayWidget : public FloatingWidget
{
    Q_OBJECT
    Q_PROPERTY (qreal opacity READ opacity WRITE setOpacity)
public:
    OverlayWidget(FloatingWidgetContainer *parent);
    ~OverlayWidget() override;
    void setHorizontalMargin(int);
    void setVerticalMargin(int);
    void setPosition(FloatingWidgetPosition pos);
    void setFadeDuration(int duration);
    void setFadeEnabled(bool mode);

public slots:
    void show();
    void hide();
    void hideAnimated();

private:
    QGraphicsOpacityEffect *opacityEffect;
    int mHorizontalMargin, mVerticalMargin;
    bool fadeEnabled;
    QPropertyAnimation *fadeAnimation;

protected slots:
    void setOpacity(qreal opacity);
    qreal opacity() const;

protected:
    void recalculateGeometry() override;
    FloatingWidgetPosition position;
};

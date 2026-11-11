// Base for floating panels: never put in a layout, just set parent and show().
// Reimplement recalculateGeometry() to set the geometry when the parent resizes.

#pragma once

#include "gui/customwidgets/floatingwidgetcontainer.h"
#include <QStyleOption>
#include <QPainter>
#include <QApplication>
#include <QDebug>

#include <QWheelEvent>

class FloatingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingWidget(FloatingWidgetContainer *parent);
    QSize containerSize();
    bool acceptKeyboardFocus() const;
    void setAcceptKeyboardFocus(bool mode);

public slots:
    void hide();

protected:
    // called whenever container rectangle changes
    // this does nothing, reimplement to use
    virtual void recalculateGeometry();
    void paintEvent(QPaintEvent *event) override;
    void setContainerSize(QSize container);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
    // size of whatever widget we are overlayed on
    QSize container;
    bool mAcceptKeyboardFocus;

private slots:
    void onContainerResized(QSize container);
};

#ifndef COLORSELECTORBUTTON_H
#define COLORSELECTORBUTTON_H

#include <QPainter>
#include <QColorDialog>
#include "gui/customwidgets/clickablelabel.h"

class ColorSelectorButton : public ClickableLabel {
    Q_OBJECT
public:
    explicit ColorSelectorButton(QWidget *parent = nullptr);

    void setColor(QColor &newColor);
    QColor color();
    void setDescription(QString text);

signals:
    void colorChanged(QColor color);
    // Emitted on Apply (and on OK if the pick differs from the last applied
    // preview) while the picker stays open; also on Cancel to undo previews.
    void colorApplied(QColor color);

protected:
    void paintEvent(QPaintEvent *e);

private slots:
    void showColorSelector();

private:
    QColor mColor;
    QString mDescription;
};

#endif // COLORSELECTORBUTTON_H

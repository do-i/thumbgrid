#include "clickablelabel.h"
#include <QMouseEvent>

ClickableLabel::ClickableLabel() {
}

ClickableLabel::ClickableLabel(QWidget *parent) : QLabel(parent) {
}

ClickableLabel::ClickableLabel(const QString &text) {
    this->setText(text);
}

ClickableLabel::ClickableLabel(const QString &text, QWidget *parent) : QLabel(parent) {
    this->setText(text);
}

// the right button belongs to a context menu
void ClickableLabel::mousePressEvent(QMouseEvent *event) {
    if(event->button() == Qt::LeftButton)
        emit clicked();
}

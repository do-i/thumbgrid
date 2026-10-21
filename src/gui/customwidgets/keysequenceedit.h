#pragma once

#include <QPushButton>
#include "shortcutbuilder.h"
#include "utils/actions.h"

class KeySequenceEdit : public QPushButton {
    Q_OBJECT
public:
    KeySequenceEdit(QWidget *parent);
    QString sequence();    
    void setText(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

    void mouseReleaseEvent(QMouseEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    bool focusNextPrevChild(bool) override;
private:
    void processEvent(QEvent *e);
    QString mSequence;
    // the click that focused the button only starts recording; it is not a binding
    bool mFocusClick = false;

signals:
    void edited();
};

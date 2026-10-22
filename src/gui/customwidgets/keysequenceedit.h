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
    // Clicks in the picture's edge zones (LMB_LeftEdge / LMB_RightEdge) can't
    // be recorded by clicking this button, so dialogs put this menu button,
    // which records them, next to it.
    QPushButton *createClickZoneButton(QWidget *parent);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

    void mouseReleaseEvent(QMouseEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;
    bool focusNextPrevChild(bool) override;
private:
    void processEvent(QEvent *e);
    void record(const QString &sequence);
    QString mSequence;
    // the click that focused the button only starts recording; it is not a binding
    bool mFocusClick = false;
    // a wheel turned with the right button held records as RMB+Wheel...; the
    // button's release that follows is part of it, not a binding of its own
    bool mWheelWhileRightHeld = false;

signals:
    void edited();
};

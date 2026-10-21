#include "keysequenceedit.h"

KeySequenceEdit::KeySequenceEdit(QWidget *parent) : QPushButton(parent), mSequence("") {
    QPushButton::setText(tr("[Enter sequence]"));
}

QString KeySequenceEdit::sequence() {
    return mSequence;
}

void KeySequenceEdit::setText(const QString &text) {
    mSequence = text;
    QPushButton::setText(mSequence);
}

void KeySequenceEdit::keyPressEvent(QKeyEvent *e) {
    processEvent(e);
}

void KeySequenceEdit::mousePressEvent(QMouseEvent *e) {
    if(mFocusClick)
        return;
    processEvent(e);
}

void KeySequenceEdit::mouseReleaseEvent(QMouseEvent *e) {
    if(mFocusClick) {
        mFocusClick = false;
        return;
    }
    processEvent(e);
}

// Qt focuses the button before delivering the press, so remember that this
// press is the one that focused it: clicking to start recording would
// otherwise bind the click itself.
void KeySequenceEdit::focusInEvent(QFocusEvent *e) {
    mFocusClick = (e->reason() == Qt::MouseFocusReason);
    QPushButton::focusInEvent(e);
}

void KeySequenceEdit::wheelEvent(QWheelEvent *e) {
    processEvent(e);
}

// block native tab-switching so we can use it in shortcuts
bool KeySequenceEdit::focusNextPrevChild(bool) {
    return false;
}

void KeySequenceEdit::processEvent(QEvent *e) {
    QInputEvent *inputEvent = dynamic_cast<QInputEvent *>(e);
    if(!inputEvent)
        return;
    QString tmp = ShortcutBuilder::fromEvent(inputEvent);
    if(!tmp.isEmpty()) {
        mSequence = tmp;
        this->setText(mSequence);
        emit edited();
    }
}

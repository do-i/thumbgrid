#include "keysequenceedit.h"

#include <QMenu>

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
    if(mWheelWhileRightHeld && e->button() == Qt::RightButton) {
        mWheelWhileRightHeld = false;
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
    if(e->buttons() & Qt::RightButton)
        mWheelWhileRightHeld = true;
}

// block native tab-switching so we can use it in shortcuts
bool KeySequenceEdit::focusNextPrevChild(bool) {
    return false;
}

void KeySequenceEdit::processEvent(QEvent *e) {
    QInputEvent *inputEvent = dynamic_cast<QInputEvent *>(e);
    if(!inputEvent)
        return;
    record(ShortcutBuilder::fromEvent(inputEvent));
}

void KeySequenceEdit::record(const QString &sequence) {
    if(sequence.isEmpty())
        return;
    mSequence = sequence;
    this->setText(mSequence);
    emit edited();
}

QPushButton *KeySequenceEdit::createClickZoneButton(QWidget *parent) {
    auto *button = new QPushButton(tr("Click zone"), parent);
    button->setAccessibleName("clickZoneButton");
    button->setAutoDefault(false);
    button->setToolTip(tr("Clicks in the picture's left and right edges (with clickable edges on)"));
    auto *menu = new QMenu(button);
    menu->addAction(tr("Left edge click"), this, [this]() { record(QStringLiteral("LMB_LeftEdge")); });
    menu->addAction(tr("Right edge click"), this, [this]() { record(QStringLiteral("LMB_RightEdge")); });
    button->setMenu(menu);
    return button;
}

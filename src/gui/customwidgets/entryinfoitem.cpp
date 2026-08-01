#include "entryinfoitem.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    layout.setContentsMargins(9,0,9,0);
    layout.setSpacing(0);
    layout.addWidget(&nameLabel);
    layout.addWidget(&valueLabel);
    layout.addWidget(&valueEdit);
    setLayout(&layout);

    nameLabel.setFixedSize(120,30);
    valueLabel.setFixedHeight(30);
    valueLabel.setMinimumWidth(40);
    valueLabel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Only ever one of the two is visible; the editor takes the label's slot so
    // an editable row lines up with the read-only rows around it.
    valueEdit.setFixedHeight(30);
    valueEdit.setMinimumWidth(40);
    valueEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    valueEdit.setFrame(false);
    valueEdit.hide();
    // editingFinished, not textChanged: a write per keystroke would rewrite the
    // file on disk while the user is still typing. It fires again on focus-out
    // after Enter, which commitEdit()'s value comparison absorbs.
    connect(&valueEdit, &QLineEdit::editingFinished, this, [this]() { commitEdit(); });

    // add some padding for easier text selection
    valueLabel.setContentsMargins(3,0,0,0);
    valueLabel.setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueLabel.setCursor(Qt::IBeamCursor);
    // Long values (e.g. paths) are elided; a right-click always copies the
    // full, un-elided value rather than whatever's currently displayed.
    valueLabel.installEventFilter(this);
}

void EntryInfoItem::setInfo(QString _name, QString _value) {
    name = std::move(_name);
    value = std::move(_value);
    nameLabel.setText(name);
    if(editable)
        valueEdit.setText(value);
    else
        updateElidedText();
};

void EntryInfoItem::setEditable(bool _editable) {
    if(editable == _editable)
        return;
    editable = _editable;
    valueLabel.setVisible(!editable);
    valueEdit.setVisible(editable);
    if(editable)
        valueEdit.setText(value);
    else
        updateElidedText();
}

void EntryInfoItem::setValuePlaceholder(const QString &text) {
    valueEdit.setPlaceholderText(text);
}

// Callable from outside so an owner can end an edit on its own terms - Qt only
// reports one when focus moves, and several ways of leaving a field (clicking
// something unfocusable, switching windows) do not move it in time.
void EntryInfoItem::commitEdit() {
    if(!editable || valueEdit.text() == value)
        return;
    value = valueEdit.text();
    emit valueEdited(value);
}

void EntryInfoItem::updateElidedText() {
    const QString elided =
        QFontMetrics(valueLabel.font()).elidedText(value, Qt::ElideMiddle, valueLabel.width());
    valueLabel.setText(elided);
    valueLabel.setToolTip(elided == value ? QString() : value);
}

void EntryInfoItem::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if(!editable)
        updateElidedText();
}

bool EntryInfoItem::eventFilter(QObject *watched, QEvent *event) {
    if(watched == &valueLabel && event->type() == QEvent::ContextMenu) {
        auto *menu = new QMenu(&valueLabel);
        QAction *copyAction = menu->addAction(tr("Copy"));
        connect(copyAction, &QAction::triggered, &valueLabel,
                [this]() { QApplication::clipboard()->setText(value); });
        menu->setAttribute(Qt::WA_DeleteOnClose);
        auto *contextEvent = static_cast<QContextMenuEvent *>(event);
        menu->popup(contextEvent->globalPos());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void EntryInfoItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

#include "entryinfoitem.h"

#include <utility>

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>

namespace {
// Three lines: enough for a realistic keyword list to be visible without the row
// dominating the tab, and it still reads as one row rather than a panel.
constexpr int kMultiLineHeight = 66;
constexpr int kRowHeight = 30;
} // namespace

EntryInfoItem::EntryInfoItem(QWidget *parent) : QWidget(parent) {
    layout.setContentsMargins(9,0,9,0);
    layout.setSpacing(0);
    layout.addWidget(&nameLabel);
    layout.addWidget(&nameEdit);
    layout.addWidget(&valueLabel);
    layout.addWidget(&valueEdit);
    layout.addWidget(&valueMultiEdit);
    layout.addWidget(&extraEdit);
    layout.addWidget(&removeBtn);
    setLayout(&layout);

    nameLabel.setFixedSize(120, kRowHeight);
    valueLabel.setFixedHeight(kRowHeight);
    valueLabel.setMinimumWidth(40);
    valueLabel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Only ever one of the two is visible; the editor takes the label's slot so
    // an editable row lines up with the read-only rows around it.
    valueEdit.setFixedHeight(kRowHeight);
    valueEdit.setMinimumWidth(40);
    valueEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    valueEdit.setFrame(false);
    valueEdit.hide();
    // editingFinished, not textChanged: a write per keystroke would rewrite the
    // file on disk while the user is still typing. It fires again on focus-out
    // after Enter, which commitEdit()'s value comparison absorbs.
    connect(&valueEdit, &QLineEdit::editingFinished, this, [this]() { commitEdit(); });

    // The name cell of a grid row. Same slot and width as the label it replaces,
    // so a grid row and a curated row line their columns up.
    nameEdit.setFixedSize(120, kRowHeight);
    nameEdit.setFrame(false);
    nameEdit.hide();
    connect(&nameEdit, &QLineEdit::editingFinished, this, [this]() { commitEdit(); });

    // QPlainTextEdit has no editingFinished; the owning dialog ends the edit on
    // focus-out through commitEdit(), the same call the line editors make.
    valueMultiEdit.setFixedHeight(kMultiLineHeight);
    valueMultiEdit.setMinimumWidth(40);
    valueMultiEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    valueMultiEdit.setFrameShape(QFrame::NoFrame);
    valueMultiEdit.setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    valueMultiEdit.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    valueMultiEdit.hide();

    extraEdit.setFixedHeight(kRowHeight);
    extraEdit.setMinimumWidth(40);
    extraEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    extraEdit.setFrame(false);
    extraEdit.hide();
    connect(&extraEdit, &QLineEdit::editingFinished, this, [this]() { commitEdit(); });

    removeBtn.setObjectName(QStringLiteral("removePropertyButton"));
    removeBtn.setText(QStringLiteral("×"));
    removeBtn.setFixedSize(24, 24);
    removeBtn.setCursor(Qt::PointingHandCursor);
    removeBtn.setAutoDefault(false);
    removeBtn.setDefault(false);
    removeBtn.hide();
    connect(&removeBtn, &QPushButton::clicked, this, [this]() { emit removeRequested(); });

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
    if(nameEditable)
        nameEdit.setText(name);
    if(multiLine)
        valueMultiEdit.setPlainText(value);
    else if(editable)
        valueEdit.setText(value);
    else
        updateElidedText();
};

void EntryInfoItem::setEditable(bool _editable) {
    if(editable == _editable)
        return;
    editable = _editable;
    valueLabel.setVisible(!editable);
    valueEdit.setVisible(editable && !multiLine);
    if(editable)
        valueEdit.setText(value);
    else
        updateElidedText();
}

void EntryInfoItem::setNameEditable(bool _editable) {
    if(nameEditable == _editable)
        return;
    nameEditable = _editable;
    nameLabel.setVisible(!nameEditable);
    nameEdit.setVisible(nameEditable);
    if(nameEditable)
        nameEdit.setText(name);
}

void EntryInfoItem::setMultiLine(bool _multiLine) {
    if(multiLine == _multiLine)
        return;
    multiLine = _multiLine;
    valueMultiEdit.setVisible(multiLine && editable);
    valueEdit.setVisible(editable && !multiLine);
    setFixedHeight(multiLine ? kMultiLineHeight : kRowHeight);
    if(multiLine)
        valueMultiEdit.setPlainText(value);
}

void EntryInfoItem::setRemovable(bool _removable) {
    if(removable == _removable)
        return;
    removable = _removable;
    removeBtn.setVisible(removable);
}

void EntryInfoItem::setValuePlaceholder(const QString &text) {
    valueEdit.setPlaceholderText(text);
    valueMultiEdit.setPlaceholderText(text);
}

void EntryInfoItem::setNamePlaceholder(const QString &text) {
    nameEdit.setPlaceholderText(text);
}

void EntryInfoItem::setNameWidth(int width) {
    nameLabel.setFixedWidth(width);
    nameEdit.setFixedWidth(width);
}

void EntryInfoItem::setExtraVisible(bool visible) {
    if(extraVisible == visible)
        return;
    extraVisible = visible;
    extraEdit.setVisible(visible);
    if(!visible) {
        extraEdit.clear();
        extra.clear();
    }
}

void EntryInfoItem::setExtraPlaceholder(const QString &text) {
    extraEdit.setPlaceholderText(text);
}

QString EntryInfoItem::currentExtra() const {
    return extraVisible ? extraEdit.text() : QString();
}

void EntryInfoItem::setValueReadOnly(bool readOnly) {
    valueEdit.setReadOnly(readOnly);
    valueMultiEdit.setReadOnly(readOnly);
    nameEdit.setReadOnly(readOnly);
    // Read-only rather than disabled: the text still has to be selectable and
    // copyable, which is the whole reason the row is shown at all.
    setProperty("valueReadOnly", readOnly);
}

QString EntryInfoItem::editorText() const {
    if(multiLine)
        return valueMultiEdit.toPlainText();
    if(editable)
        return valueEdit.text();
    return value;
}

QString EntryInfoItem::currentName() const {
    return nameEditable ? nameEdit.text() : name;
}

QString EntryInfoItem::currentValue() const {
    return editorText();
}

bool EntryInfoItem::isBlank() const {
    return currentName().trimmed().isEmpty() && currentValue().trimmed().isEmpty();
}

// Callable from outside so an owner can end an edit on its own terms - Qt only
// reports one when focus moves, and several ways of leaving a field (clicking
// something unfocusable, switching windows) do not move it in time.
void EntryInfoItem::commitEdit() {
    if(!editable)
        return;
    const QString newValue = editorText();
    // A grid row commits both cells at once. Committing per field would write a
    // key-only property the moment focus left the key cell, then rewrite it when
    // the value arrived - two writes for one entry (docs/2026-08-01-001 §5).
    if(nameEditable) {
        const QString newName = nameEdit.text();
        const QString newExtra = currentExtra();
        if(newName == name && newValue == value && newExtra == extra)
            return;
        name = newName;
        value = newValue;
        extra = newExtra;
        emit rowEdited(name, value);
        return;
    }
    if(newValue == value)
        return;
    value = newValue;
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

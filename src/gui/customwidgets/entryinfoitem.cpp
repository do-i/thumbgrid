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
    setLayout(&layout);

    nameLabel.setFixedSize(120,30);
    valueLabel.setFixedHeight(30);
    valueLabel.setMinimumWidth(40);
    valueLabel.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

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
    updateElidedText();
};

void EntryInfoItem::updateElidedText() {
    const QString elided =
        QFontMetrics(valueLabel.font()).elidedText(value, Qt::ElideMiddle, valueLabel.width());
    valueLabel.setText(elided);
    valueLabel.setToolTip(elided == value ? QString() : value);
}

void EntryInfoItem::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
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

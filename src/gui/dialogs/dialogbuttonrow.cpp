#include "dialogbuttonrow.h"

#include <QBoxLayout>
#include <QDialog>
#include <QIcon>
#include <QPushButton>

namespace DialogButtonRow {

void styleButtons(QPushButton *ok, QPushButton *cancel, QPushButton *apply) {
    ok->setDefault(true);
    for(QPushButton *b : {ok, cancel, apply}) {
        if(!b)
            continue;
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(QIcon());
    }
}

Buttons add(QDialog *dialog, QBoxLayout *layout, bool withApply) {
    auto *row = new QHBoxLayout();
    row->addStretch();
    Buttons b{new QPushButton(QObject::tr("OK"), dialog),
              new QPushButton(QObject::tr("Cancel"), dialog),
              withApply ? new QPushButton(QObject::tr("Apply"), dialog) : nullptr};
    styleButtons(b.ok, b.cancel, b.apply);
    QObject::connect(b.ok, &QPushButton::clicked, dialog, &QDialog::accept);
    QObject::connect(b.cancel, &QPushButton::clicked, dialog, &QDialog::reject);
    row->addWidget(b.ok);
    row->addWidget(b.cancel);
    if(b.apply)
        row->addWidget(b.apply);
    layout->addLayout(row);
    return b;
}

} // namespace DialogButtonRow

#include "moveprogressoverlay.h"

#include <QHBoxLayout>
#include <QLocale>

MoveProgressOverlay::MoveProgressOverlay(FloatingWidgetContainer *parent)
    : OverlayWidget(parent),
      label(new QLabel(this)),
      bar(new QProgressBar(this)),
      cancelButton(new QPushButton(tr("Cancel"), this))
{
    label->setProperty("styleClass", QStringLiteral("MoveProgressLabel"));
    bar->setProperty("styleClass", QStringLiteral("MoveProgressBar"));
    cancelButton->setProperty("styleClass", QStringLiteral("MoveProgressCancel"));
    bar->setTextVisible(false);
    bar->setFixedSize(120, 4);
    setFocusPolicy(Qt::NoFocus);
    cancelButton->setFocusPolicy(Qt::NoFocus);
    cancelButton->setAutoDefault(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 5, 5);
    layout->setSpacing(10);
    layout->addWidget(label);
    layout->addWidget(bar, 0, Qt::AlignVCenter);
    layout->addWidget(cancelButton);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        setCancelling();
        emit cancelClicked();
    });

    // default margins, like FloatingMessage: clear of the window's status footer
    setPosition(FloatingWidgetPosition::BOTTOMRIGHT);
    setFadeEnabled(true);
    reset();
    hide();
    if(parent)
        setContainerSize(parent->size());
}

void MoveProgressOverlay::reset() {
    cancelButton->setEnabled(true);
    cancelButton->setText(tr("Cancel"));
    setProgress(0, -1);
}

void MoveProgressOverlay::setProgress(qint64 done, qint64 total) {
    if(!cancelButton->isEnabled()) // keep "Cancelling…" until the worker stops
        return;
    QLocale locale;
    if(total < 0) {
        label->setText(tr("Moving…"));
        bar->setRange(0, 0); // busy indicator
    } else {
        label->setText(tr("Moving %1 / %2 files").arg(locale.toString(done), locale.toString(total)));
        // QProgressBar is int-based; scale to permille so huge counts never overflow
        bar->setRange(0, 1000);
        bar->setValue(total > 0 ? static_cast<int>(done * 1000 / total) : 0);
    }
    recalculateGeometry();
}

void MoveProgressOverlay::setCancelling() {
    cancelButton->setEnabled(false);
    label->setText(tr("Cancelling after the current file…"));
    recalculateGeometry();
}

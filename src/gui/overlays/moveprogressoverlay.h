#pragma once

#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "gui/customwidgets/overlaywidget.h"

// Progress of a background move across filesystems, with a Cancel button. Sits in the bottom-right
// corner, clear of the bottom-center floating messages, and never takes keyboard focus so browsing
// continues while files move.
class MoveProgressOverlay : public OverlayWidget {
    Q_OBJECT
public:
    explicit MoveProgressOverlay(FloatingWidgetContainer *parent);
    // total < 0 while the worker is still counting
    void setProgress(qint64 done, qint64 total);
    // after Cancel: the worker finishes the file it is on, then stops
    void setCancelling();
    // a fresh move: re-arm Cancel
    void reset();

signals:
    void cancelClicked();

private:
    QLabel *label;
    QProgressBar *bar;
    QPushButton *cancelButton;
};

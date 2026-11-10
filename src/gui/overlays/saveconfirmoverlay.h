#pragma once

#include "gui/customwidgets/overlaywidget.h"
#include "settings.h"
#include <QPushButton>
#include <QTimer>

namespace Ui {
class SaveConfirmOverlay;
}

// Shown while the current image has unsaved edits. It starts as a single
// pencil button; clicking it toggles a row of Save / Save as / Undo all, and a
// chevron at the end folds it back. It stays out of the way: shortly after
// appearing it dims, and comes back on hover, on a new edit, or while saving.
class SaveConfirmOverlay : public OverlayWidget
{
    Q_OBJECT
public:
    explicit SaveConfirmOverlay(FloatingWidgetContainer *parent = nullptr);
    ~SaveConfirmOverlay() override;
    // while the edit is being written: say so, and take no further choice
    void setBusy(bool busy);
    bool isDimmed() const;
    bool isExpanded() const;
    void setExpanded(bool expanded);

public slots:
    void show();
    void hide();

signals:
    void saveClicked();
    void saveAsClicked();
    void discardClicked();

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void readSettings();
    void dim();
private:
    void wake();
    void scheduleDim();
    void updateToolTips();
    void relayout();
    Ui::SaveConfirmOverlay *ui;
    QTimer dimTimer;
    QPropertyAnimation *dimAnimation;
    bool busy = false;
    bool dimmed = false;
    bool expanded = false;
};

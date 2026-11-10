#pragma once

#include <cstdint>
#include "gui/customwidgets/iconbutton.h"
#include "components/actionmanager/actionmanager.h"

enum TriggerMode : std::uint8_t {
    PressTrigger,
    ClickTrigger
};

class ActionButton : public IconButton {
public:
    ActionButton(QWidget *parent = nullptr);
    ActionButton(QString _actionName, QString _iconPath, QWidget *parent = nullptr);
    ActionButton(QString _actionName, QString _iconPath, int _size, QWidget *parent = nullptr);
    void setAction(QString _actionName);
    void setTriggerMode(TriggerMode mode);
    // Static tooltip "label (shortcut)", re-set only when the bindings change.
    void setToolTipLabel(const QString &label, const QString &shortcutAction, ViewMode context);

private:
    void refreshToolTip();
    QString mToolTipLabel;
    QString mToolTipAction;
    ViewMode mToolTipContext = MODE_DOCUMENT;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QString actionName;
    TriggerMode mTriggerMode;
};

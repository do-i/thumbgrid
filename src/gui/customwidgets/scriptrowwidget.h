#pragma once

#include <QWidget>
#include <QMouseEvent>

// Row widget shown inside the Settings > Scripts list via
// QListWidget::setItemWidget(). It hosts the script name label plus the per-row
// edit/delete IconButtons. The container stays opaque to mouse events so its
// child buttons receive their own clicks; single/double clicks on the rest of
// the row are re-emitted so the owner can drive selection and edit, keeping the
// native double-click-to-edit behaviour.
class ScriptRowWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScriptRowWidget(QWidget *parent = nullptr) : QWidget(parent) {}

signals:
    void clicked();
    void doubleClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        emit clicked();
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        emit doubleClicked();
        QWidget::mouseDoubleClickEvent(event);
    }
};

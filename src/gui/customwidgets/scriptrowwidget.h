#pragma once

#include <QWidget>
#include <QMouseEvent>

// Settings > Scripts row: name plus edit/delete buttons; re-emits clicks on the rest of the row.
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

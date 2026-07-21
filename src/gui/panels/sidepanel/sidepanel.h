#pragma once

#include <QWidget>
#include <QDebug>
#include <QStyleOption>
#include <QPainter>
#include <QVBoxLayout>
#include <QCloseEvent>
#include "gui/customwidgets/sidepanelwidget.h"

namespace Ui {
class SidePanel;
}

class SidePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SidePanel(QWidget *parent = nullptr);
    ~SidePanel() override;

    void setWidget(SidePanelWidget *w);
    SidePanelWidget* widget();

public slots:
    void show();
    void hide();

signals:
    void closed();

protected:
    void paintEvent(QPaintEvent *) override;
    void closeEvent(QCloseEvent *event) override;
private:
    Ui::SidePanel *ui;
    SidePanelWidget *mWidget;
};

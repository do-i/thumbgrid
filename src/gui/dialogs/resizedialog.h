#pragma once

#include <QCheckBox>
#include <QDebug>
#include <QScreen>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include "utils/resizecopy.h"

namespace Ui {
    class ResizeDialog;
}

class ResizeDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ResizeDialog(QSize initialSize, QWidget *parent = nullptr);
    ~ResizeDialog() override;
    QSize newSize();
    // The choice as a per-image rule: percentage stays a percentage, a W x H
    // with aspect kept becomes "fit within" so other aspect ratios are not
    // stretched when the same spec is applied to several images.
    ResizeSpec spec() const;
    // Retitles the dialog for the grid flow, which writes new files instead
    // of editing the open image.
    void setCopyMode(int imageCount);

public slots:
    int exec() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Ui::ResizeDialog *ui;
    QSize originalSize, targetSize, desktopSize;
    void updateToTargetValues();
    int lastEdited; // 0 - width, 1 - height
    void resetResCheckBox();

private slots:
    void widthChanged(int);
    void heightChanged(int);
    void percentChanged(double);
    void sizeSelect();

    void setCommonResolution(int);
    void reset();
    void fitDesktop();
    void fillDesktop();
    void onAspectRatioCheckbox();
    void onPercentageRadioButton();
    void onAbsoluteSizeRadioButton();
signals:
    void sizeSelected(QSize);
    void specSelected(ResizeSpec);
};

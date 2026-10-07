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
    // Switches to the grid flow, which writes new files instead of editing the
    // open image: adds Long edge and Shrink only, and with more than one image
    // turns W x H into a "fit within" box (an exact size would stretch images
    // of other shapes). specSelected then fires on OK instead of sizeSelected.
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
    void setupFilterCombo();
    ScalingFilter selectedFilter() const;

    bool copyMode = false;
    bool multiImage = false;
    // copy-mode only controls, built in code next to the .ui ones
    QRadioButton *byLongEdge;
    QLabel *longEdgeLabel;
    QSpinBox *longEdge;
    QCheckBox *shrinkOnly;

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
    void onLongEdgeRadioButton();
signals:
    void sizeSelected(QSize);
    void specSelected(ResizeSpec);
};

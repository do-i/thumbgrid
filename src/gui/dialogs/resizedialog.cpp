#include "resizedialog.h"
#include "ui_resizedialog.h"

ResizeDialog::ResizeDialog(QSize originalSize,  QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResizeDialog),
    lastEdited(0)
{
    ui->setupUi(this);
    setWindowModality(Qt::ApplicationModal);
    ui->percent->setFocus();

    this->originalSize = originalSize;
    targetSize = originalSize;

    ui->width->setValue(originalSize.width());
    ui->height->setValue(originalSize.height());

    ui->resetButton->setText(tr("Reset:") + " " +
                             QString::number(originalSize.width()) +
                             " x " +
                             QString::number(originalSize.height()));

    // Copy-mode controls. Parented to the frame that holds the other two radio
    // buttons so all three stay mutually exclusive; hidden until setCopyMode().
    byLongEdge = new QRadioButton(tr("By Long Edge:"), ui->resFrame);
    byLongEdge->setObjectName("byLongEdge");
    ui->verticalLayout_6->addWidget(byLongEdge);
    longEdgeLabel = new QLabel(tr("Long edge:"), ui->resFrame);
    longEdge = new QSpinBox(ui->resFrame);
    longEdge->setObjectName("longEdge");
    longEdge->setRange(1, 65535);
    longEdge->setSuffix(" px");
    longEdge->setValue(qMax(originalSize.width(), originalSize.height()));
    longEdge->setEnabled(false);
    int row = ui->resGridLayout->rowCount();
    ui->resGridLayout->addWidget(longEdgeLabel, row, 0);
    ui->resGridLayout->addWidget(longEdge, row, 1);
    shrinkOnly = new QCheckBox(tr("Only shrink (skip images already smaller)"), ui->resFrame);
    shrinkOnly->setObjectName("shrinkOnly");
    shrinkOnly->setChecked(true);
    ui->verticalLayout_3->insertWidget(ui->verticalLayout_3->indexOf(ui->keepAspectRatio) + 1, shrinkOnly);
    for(QWidget *w : {static_cast<QWidget *>(byLongEdge), static_cast<QWidget *>(longEdgeLabel),
                      static_cast<QWidget *>(longEdge), static_cast<QWidget *>(shrinkOnly)})
        w->hide();

    setupFilterCombo();

    desktopSize = qApp->primaryScreen()->size();
    // toggled fires for the button being switched off as well; react only to
    // the one switched on, or the old mode re-enables its own controls
    connect(ui->byPercentage, &QRadioButton::toggled, this, [this](bool on) { if(on) onPercentageRadioButton(); });
    connect(ui->byAbsoluteSize, &QRadioButton::toggled, this, [this](bool on) { if(on) onAbsoluteSizeRadioButton(); });
    connect(byLongEdge, &QRadioButton::toggled, this, [this](bool on) { if(on) onLongEdgeRadioButton(); });
    connect(ui->percent, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ResizeDialog::percentChanged);
    connect(ui->width,  qOverload<int>(&QSpinBox::valueChanged), this, &ResizeDialog::widthChanged);
    connect(ui->height, qOverload<int>(&QSpinBox::valueChanged), this, &ResizeDialog::heightChanged);
    connect(ui->keepAspectRatio, &QCheckBox::toggled, this, &ResizeDialog::onAspectRatioCheckbox);
    connect(ui->resComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, &ResizeDialog::setCommonResolution);
    connect(ui->fitDesktopButton, &QPushButton::pressed, this, &ResizeDialog::fitDesktop);
    connect(ui->fillDesktopButton, &QPushButton::pressed, this, &ResizeDialog::fillDesktop);
    connect(ui->resetButton, &QPushButton::pressed, this, &ResizeDialog::reset);
    connect(ui->cancelButton, &QPushButton::pressed, this, &ResizeDialog::reject);
    connect(ui->okButton, &QPushButton::pressed, this, &ResizeDialog::sizeSelect);
}

ResizeDialog::~ResizeDialog() {
    delete ui;
}

void ResizeDialog::sizeSelect() {
    settings->setResizeFilter(selectedFilter());
    // In copy mode "no change" is decided per image (ResizeCopy::targetSize),
    // since the seed image's size says nothing about the others.
    if(copyMode)
        emit specSelected(spec());
    else if(targetSize != originalSize)
        emit sizeSelected(targetSize);
    this->accept();
}

void ResizeDialog::setupFilterCombo() {
    // The .ui ships a disabled placeholder; fill it with what ImageLib::scaled
    // can actually do in this build.
    ui->comboBox->clear();
    ui->comboBox->addItem(tr("Nearest"), QI_FILTER_NEAREST);
    ui->comboBox->addItem(tr("Bilinear"), QI_FILTER_BILINEAR);
#ifdef USE_OPENCV
    ui->comboBox->addItem(tr("Bilinear + sharpen"), QI_FILTER_CV_BILINEAR_SHARPEN);
    ui->comboBox->addItem(tr("Bicubic"), QI_FILTER_CV_CUBIC);
    ui->comboBox->addItem(tr("Bicubic + sharpen"), QI_FILTER_CV_CUBIC_SHARPEN);
#endif
    int index = ui->comboBox->findData(settings->resizeFilter());
    ui->comboBox->setCurrentIndex(index < 0 ? 1 : index);
    ui->comboBox->setEnabled(true);
    ui->label_4->setEnabled(true);
}

ScalingFilter ResizeDialog::selectedFilter() const {
    return static_cast<ScalingFilter>(ui->comboBox->currentData().toInt());
}

void ResizeDialog::setCommonResolution(int index) {
    QSize res;
    switch(index) {
        case 1: res = QSize(1366, 768); break;
        case 2: res = QSize(1440, 900); break;
        case 3: res = QSize(1440, 1050); break;
        case 4: res = QSize(1600, 1200); break;
        case 5: res = QSize(1920, 1080); break;
        case 6: res = QSize(1920, 1200); break;
        case 7: res = QSize(2560, 1080); break;
        case 8: res = QSize(2560, 1440); break;
        case 9: res = QSize(2560, 1600); break;
        case 10: res = QSize(3840, 1600); break;
        case 11: res = QSize(3840, 2160); break;
        default: res = originalSize; break;
    }
    if(multiImage)
        targetSize = res; // the box itself; each image is fitted into it later
    else if(ui->keepAspectRatio->isChecked())
        targetSize = originalSize.scaled(res, Qt::KeepAspectRatio);
    else
        targetSize = originalSize.scaled(res, Qt::IgnoreAspectRatio);
    updateToTargetValues();
}

ResizeSpec ResizeDialog::spec() const {
    ResizeSpec spec;
    if(ui->byPercentage->isChecked()) {
        spec.mode = ResizeSpec::Percent;
        spec.percent = ui->percent->value();
    } else if(byLongEdge->isChecked()) {
        spec.mode = ResizeSpec::LongEdge;
        spec.longEdge = longEdge->value();
    } else {
        // with several images the W x H is a box to fit into, never a stretch
        bool fit = multiImage || ui->keepAspectRatio->isChecked();
        spec.mode = fit ? ResizeSpec::FitWithin : ResizeSpec::Exact;
        spec.size = targetSize;
    }
    // the in-place picture view edit has no skip rule: it does what was asked
    spec.shrinkOnly = copyMode && shrinkOnly->isChecked();
    spec.filter = selectedFilter();
    return spec;
}

void ResizeDialog::setCopyMode(int imageCount) {
    copyMode = true;
    multiImage = imageCount > 1;
    byLongEdge->show();
    longEdgeLabel->show();
    longEdge->show();
    shrinkOnly->show();
    if(multiImage) {
        setWindowTitle(tr("Resize %1 images (saves copies)").arg(imageCount));
        ui->byAbsoluteSize->setText(tr("Fit Within:"));
        // W and H become the two independent sides of the box
        ui->keepAspectRatio->blockSignals(true);
        ui->keepAspectRatio->setChecked(false);
        ui->keepAspectRatio->blockSignals(false);
        ui->keepAspectRatio->hide();
        // "expanding" past a box has no per-image meaning
        ui->fillDesktopButton->hide();
    } else {
        setWindowTitle(tr("Resize (saves a copy)"));
    }
}

QSize ResizeDialog::newSize() {
    return targetSize;
}

void ResizeDialog::widthChanged(int newWidth) {
    lastEdited = 0;
    float factor = static_cast<float>(newWidth) / originalSize.width();
    targetSize.setWidth(newWidth);
    if(ui->keepAspectRatio->isChecked()) {
        targetSize.setHeight(static_cast<int>(originalSize.height() * factor));
    }
    updateToTargetValues();
}

void ResizeDialog::heightChanged(int newHeight) {
    lastEdited = 1;
    float factor = static_cast<float>(newHeight) / originalSize.height();
    targetSize.setHeight(newHeight);
    if(ui->keepAspectRatio->isChecked()) {
        targetSize.setWidth(static_cast<int>(originalSize.width() * factor));
    }
    updateToTargetValues();
}

void ResizeDialog::updateToTargetValues() {
    ui->width->blockSignals(true);
    ui->height->blockSignals(true);
    ui->width->setValue(targetSize.width());
    ui->height->setValue(targetSize.height());
    ui->width->blockSignals(false);
    ui->height->blockSignals(false);
}

void ResizeDialog::fitDesktop() {
    if(multiImage)
        targetSize = desktopSize; // the box itself, as with the common sizes
    else
        targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatio);
    updateToTargetValues();
}

void ResizeDialog::fillDesktop() {
    targetSize = originalSize.scaled(desktopSize, Qt::KeepAspectRatioByExpanding);
    updateToTargetValues();
}

void ResizeDialog::onAspectRatioCheckbox() {
    resetResCheckBox();
    (lastEdited)?heightChanged(ui->height->value()):widthChanged(ui->width->value());
}

void ResizeDialog::onAbsoluteSizeRadioButton() {
    ui->width->blockSignals(true);
    ui->height->blockSignals(true);
    ui->percent->blockSignals(true);
    ui->keepAspectRatio->blockSignals(true);

    ui->width->setEnabled(true);
    ui->height->setEnabled(true);
    ui->percent->setEnabled(false);
    longEdge->setEnabled(false);
    ui->keepAspectRatio->setEnabled(true);
    // percentage mode forces aspect on; a multi-image box must stay unlinked
    if(multiImage)
        ui->keepAspectRatio->setChecked(false);

    ui->width->blockSignals(false);
    ui->height->blockSignals(false);
    ui->percent->blockSignals(false);
    ui->keepAspectRatio->blockSignals(false);
}

void ResizeDialog::onPercentageRadioButton() {
    ui->width->blockSignals(true);
    ui->height->blockSignals(true);
    ui->percent->blockSignals(true);
    ui->keepAspectRatio->blockSignals(true);

    ui->width->setEnabled(false);
    ui->height->setEnabled(false);
    ui->percent->setEnabled(true);
    longEdge->setEnabled(false);
    ui->keepAspectRatio->setChecked(true);
    ui->keepAspectRatio->setEnabled(false);
    percentChanged(ui->percent->value());

    ui->width->blockSignals(false);
    ui->height->blockSignals(false);
    ui->percent->blockSignals(false);
    ui->keepAspectRatio->blockSignals(false);
}

void ResizeDialog::onLongEdgeRadioButton() {
    ui->width->setEnabled(false);
    ui->height->setEnabled(false);
    ui->percent->setEnabled(false);
    ui->keepAspectRatio->setEnabled(false);
    longEdge->setEnabled(true);
    longEdge->setFocus();
}

void ResizeDialog::resetResCheckBox() {
    ui->resComboBox->blockSignals(true);
    ui->resComboBox->setCurrentIndex(0);
    ui->resComboBox->blockSignals(false);
}

void ResizeDialog::percentChanged(double newPercent) {
    double scale = newPercent / 100.;
    targetSize.setWidth(originalSize.width()*scale);
    targetSize.setHeight(originalSize.height()*scale);

    updateToTargetValues();
}

void ResizeDialog::keyPressEvent(QKeyEvent *event) {
    if((event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
        sizeSelect();
    } else if(event->key() == Qt::Key_Escape) {
        reject();
    } else {
        event->ignore();
    }
}

void ResizeDialog::reset() {
    resetResCheckBox();
    targetSize = originalSize;
    updateToTargetValues();
}

int ResizeDialog::exec() {
    resize(sizeHint());
    return QDialog::exec();
}

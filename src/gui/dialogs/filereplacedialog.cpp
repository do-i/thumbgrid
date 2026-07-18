#include "filereplacedialog.h"
#include "ui_filereplacedialog.h"

#include <QPainter>
#include <QStyleOption>
#include <QScreen>
#include <QMouseEvent>
#include <QWindow>

FileReplaceDialog::FileReplaceDialog(QWidget *parent) : QDialog(parent), ui(new Ui::FileReplaceDialog) {
    ui->setupUi(this);
    // Frameless themed chrome, matching CustomMessageBox / ContextMenu, so the
    // conflict prompt no longer shows a native OS title bar.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // Inset the content so it clears the rounded corners drawn in paintEvent.
    if(auto *lay = layout())
        lay->setContentsMargins(20, 18, 20, 16);
    ui->titleLabel->setAccessibleName(QStringLiteral("DialogTitle"));
    ui->yesButton->setDefault(true);
    multi = false;
    connect(ui->yesButton, &QPushButton::clicked, this, &FileReplaceDialog::onYesClicked);
    connect(ui->noButton, &QPushButton::clicked, this, &FileReplaceDialog::onNoClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &FileReplaceDialog::onCancelClicked);
}

FileReplaceDialog::~FileReplaceDialog() {
    delete ui;
}

void FileReplaceDialog::setSource(const QString& src) {
    ui->srcLabel->setText(src);
}

void FileReplaceDialog::setDestination(const QString& dst) {
    ui->dstLabel->setText(dst);
}

void FileReplaceDialog::setMode(FileReplaceMode mode) {
    if(mode == FILE_TO_FILE) {
        setWindowTitle("File already exists");
        ui->titleLabel->setText("Replace destination file?");
    } else if(mode == DIR_TO_DIR) {
        setWindowTitle("Directory already exists");
        ui->titleLabel->setText("Merge directories?");
    } else if(mode == DIR_TO_FILE) {
        setWindowTitle("Destination already exists");
        ui->titleLabel->setText("There is a file with that name. Replace?");
    } else { // FILE_TO_DIR
        setWindowTitle("Destination already exists");
        ui->titleLabel->setText("There is a folder with that name. Replace?");
    }
}

void FileReplaceDialog::setMulti(bool _multi) {
    multi = _multi;
    ui->applyAllCheckBox->setVisible(multi);
}

DialogResult FileReplaceDialog::getResult() {
    return result;
}

void FileReplaceDialog::onYesClicked() {
    result.yes = true;
    result.all = ui->applyAllCheckBox->isChecked();
    result.cancel = false;
    this->close();
}

void FileReplaceDialog::onNoClicked() {
    result.yes = false;
    result.all = ui->applyAllCheckBox->isChecked();
    result.cancel = false;
    this->close();
}

void FileReplaceDialog::onCancelClicked() {
    result.cancel = true;
    this->close();
}

void FileReplaceDialog::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    // Let the stylesheet paint the rounded, themed background/border.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void FileReplaceDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    if(QWidget *ref = parentWidget()) {
        const QRect area = ref->frameGeometry();
        move(area.center() - QPoint(width() / 2, height() / 2));
    }
}

void FileReplaceDialog::mousePressEvent(QMouseEvent *event) {
    // Frameless window has no title bar to grab, so allow dragging from the
    // dialog background itself (child widgets consume their own presses).
    if(event->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        return;
    }
    QDialog::mousePressEvent(event);
}

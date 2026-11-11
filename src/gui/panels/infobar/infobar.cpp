#include "infobar.h"
#include "ui_infobar.h"
#include "settings.h"

InfoBar::InfoBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::InfoBar)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    ui->path->setText(tr("No file opened."));
    connect(settings, &Settings::settingsChanged, this, &InfoBar::readSettings);
    readSettings();
}

InfoBar::~InfoBar() {
    delete ui;
}

void InfoBar::setInfo(const QString& position, const QString& fileName, const QString& info) {
    ui->index->setText(position);
    ui->path->setText(fileName);
    ui->info->setText(info);
}

void InfoBar::setStatusText(const QString& text) {
    ui->index->clear();
    ui->path->setText(text);
    ui->info->clear();
}

void InfoBar::readSettings() {
    const ColorScheme &scheme = settings->colorScheme();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, scheme.widget);
    pal.setColor(QPalette::WindowText, scheme.text_hc);
    setPalette(pal);

    ui->index->setPalette(pal);
    ui->path->setPalette(pal);
    ui->info->setPalette(pal);
}

void InfoBar::wheelEvent(QWheelEvent *event) {
    event->accept();
}

void InfoBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

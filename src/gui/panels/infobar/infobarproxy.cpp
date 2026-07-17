#include "infobarproxy.h"
#include "settings.h"

InfoBarProxy::InfoBarProxy(QWidget *parent) : QWidget(parent), infoBar(nullptr) {
    setAccessibleName("InfoBarProxy");
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);
    this->setMinimumHeight(23);
    this->setMaximumHeight(23);
    layout.setContentsMargins(0,0,0,0);
    setLayout(&layout);
    connect(settings, &Settings::settingsChanged, this, &InfoBarProxy::readSettings);
    readSettings();
}

InfoBarProxy::~InfoBarProxy() {
    if(infoBar)
        infoBar->deleteLater();
}

void InfoBarProxy::setInfo(const QString& position, const QString& fileName, const QString& info) {
    if(infoBar) {
        infoBar->setInfo(position, fileName, info);
    } else {
        stateBuf.position = position;
        stateBuf.fileName = fileName;
        stateBuf.info = info;
    }
}

void InfoBarProxy::setStatusText(const QString& text) {
    if(infoBar) {
        infoBar->setStatusText(text);
    } else {
        stateBuf.position = "";
        stateBuf.fileName = text;
        stateBuf.info = "";
    }
}

void InfoBarProxy::readSettings() {
    const QColor controlBackground = settings->colorScheme().widget;
    setStyleSheet(QString("background-color: %1;").arg(controlBackground.name()));

    QPalette pal = palette();
    pal.setColor(QPalette::Window, controlBackground);
    setPalette(pal);
}

void InfoBarProxy::init() {
    if(infoBar)
        return;
    infoBar = new InfoBar(this);
    setFocusProxy(infoBar);
    layout.addWidget(infoBar);
    setLayout(&layout);
    if(!stateBuf.fileName.isEmpty())
        infoBar->setInfo(stateBuf.position, stateBuf.fileName, stateBuf.info);
}

void InfoBarProxy::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

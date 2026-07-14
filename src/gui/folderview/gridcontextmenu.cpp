#include "gridcontextmenu.h"

#include <QScreen>
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>
#include <QKeyEvent>

GridContextMenu::GridContextMenu(QWidget *parent) :
    QWidget(parent)
{
    setWindowFlags(Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoMousePropagation, true);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 4, 0, 4);
    rootLayout->setSpacing(0);

    stack = new QStackedWidget(this);
    rootLayout->addWidget(stack);

    // ------------------------------------------------------------------ main
    auto *mainPage = new QWidget(stack);
    auto *mainLayout = new QVBoxLayout(mainPage);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);

    convertItem = makeItem(tr("Convert to..."), ":/res/icons/common/menuitem/image-crop16.png");
    convertItem->setPassthroughClicks(false); // keep the menu open, just switch page
    connect(convertItem, &ContextMenuItem::pressed, this, &GridContextMenu::switchToConvertPage);
    mainLayout->addWidget(convertItem);

    // thin divider between the file action and the view toggles
    auto *separator = new QWidget(mainPage);
    separator->setAccessibleName("HLineSeparator");
    separator->setFixedHeight(1);
    separator->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *sepLayout = new QVBoxLayout();
    sepLayout->setContentsMargins(11, 4, 11, 4);
    sepLayout->addWidget(separator);
    mainLayout->addLayout(sepLayout);

    // view toggles - each drives an action so the shortcut text fills in itself
    auto *topBar = makeItem(tr("Header title bar"), ":/res/icons/common/menuitem/titlebar16.png");
    topBar->setAction("toggleFolderViewTopBar");
    mainLayout->addWidget(topBar);

    auto *sidePanel = makeItem(tr("Left side panel"), ":/res/icons/common/menuitem/sidepanel16.png");
    sidePanel->setAction("togglePlacesPanel");
    mainLayout->addWidget(sidePanel);

    auto *statusBar = makeItem(tr("Bottom status bar"), ":/res/icons/common/menuitem/statusbar16.png");
    statusBar->setAction("toggleStatusFooter");
    mainLayout->addWidget(statusBar);

    // thin divider between the view toggles and app-level actions
    auto *separator2 = new QWidget(mainPage);
    separator2->setAccessibleName("HLineSeparator");
    separator2->setFixedHeight(1);
    separator2->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *sepLayout2 = new QVBoxLayout();
    sepLayout2->setContentsMargins(11, 4, 11, 4);
    sepLayout2->addWidget(separator2);
    mainLayout->addLayout(sepLayout2);

    auto *settingsItem = makeItem(tr("Settings"), ":/res/icons/common/menuitem/settings16.png");
    settingsItem->setAction("openSettings");
    mainLayout->addWidget(settingsItem);

    stack->addWidget(mainPage);

    // --------------------------------------------------------------- convert
    auto *convertPage = new QWidget(stack);
    auto *convertLayout = new QVBoxLayout(convertPage);
    convertLayout->setContentsMargins(0, 0, 0, 0);
    convertLayout->setSpacing(2);

    auto *backItem = makeItem(tr("Back"), ":/res/icons/common/menuitem/back16.png");
    backItem->setPassthroughClicks(false);
    connect(backItem, &ContextMenuItem::pressed, this, &GridContextMenu::switchToMainPage);
    convertLayout->addWidget(backItem);

    addConvertFormat(convertLayout, "JPEG", "jpg");
    addConvertFormat(convertLayout, "PNG",  "png");
    addConvertFormat(convertLayout, "WebP", "webp");

    stack->addWidget(convertPage);

    switchToMainPage();
    adjustSize();
    hide();
}

ContextMenuItem *GridContextMenu::makeItem(const QString &text, const QString &iconPath) {
    auto *item = new ContextMenuItem(this);
    item->setText(text);
    item->setIconPath(iconPath);
    return item;
}

void GridContextMenu::addConvertFormat(QVBoxLayout *layout, const QString &label, const QString &format) {
    auto *item = makeItem(label, ":/res/icons/common/menuitem/image-crop16.png");
    connect(item, &ContextMenuItem::pressed, this, [this, format]() {
        emit convertFormatRequested(format);
    });
    layout->addWidget(item);
}

void GridContextMenu::setImageEntriesEnabled(bool mode) {
    convertItem->setEnabled(mode);
}

void GridContextMenu::switchToMainPage() {
    stack->setCurrentIndex(0);
}

void GridContextMenu::switchToConvertPage() {
    stack->setCurrentIndex(1);
}

void GridContextMenu::showAt(QPoint pos) {
    switchToMainPage();
    QRect geom = geometry();
    geom.moveTopLeft(pos);
    clampToScreen(geom);
    setGeometry(geom);
    show();
}

void GridContextMenu::clampToScreen(QRect &geom) {
    auto screen = QGuiApplication::screenAt(cursor().pos());
    if(!screen)
        screen = QGuiApplication::primaryScreen();
    if(screen) {
        if(geom.bottom() > screen->geometry().bottom())
            geom.moveBottom(cursor().pos().y());
        if(geom.right() > screen->geometry().right())
            geom.moveRight(screen->geometry().right());
    }
}

void GridContextMenu::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    hide();
}

void GridContextMenu::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void GridContextMenu::keyPressEvent(QKeyEvent *event) {
    quint32 nativeScanCode = event->nativeScanCode();
    QString key = actionManager->keyForNativeScancode(nativeScanCode);
    if(key == "Esc")
        hide();
    else
        actionManager->processEvent(event);
}

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
    convertItem->setObjectName("menuConvert");
    convertItem->setPassthroughClicks(false); // keep the menu open, just switch page
    connect(convertItem, &ContextMenuItem::pressed, this, &GridContextMenu::switchToConvertPage);
    mainLayout->addWidget(convertItem);

    renameItem = makeItem(tr("Rename..."), ":/res/icons/common/overlay/edit16.png");
    renameItem->setObjectName("menuRename");
    renameItem->setAction("renameFile");
    mainLayout->addWidget(renameItem);

    moveItem = makeItem(tr("Move to..."), ":/res/icons/common/menuitem/move16.png");
    moveItem->setObjectName("menuMove");
    moveItem->setAction("moveFile");
    mainLayout->addWidget(moveItem);

    trashItem = makeItem(tr("Move to trash"), ":/res/icons/common/menuitem/trash16.png");
    trashItem->setObjectName("menuTrash");
    trashItem->setAction("moveToTrash");
    mainLayout->addWidget(trashItem);

    addSeparator(mainPage, mainLayout);

    // destructive and not undoable; relies on the confirmation dialog downstream
    deleteItem = makeItem(tr("Delete permanently"), ":/res/icons/common/menuitem/trash16.png");
    deleteItem->setObjectName("menuDelete");
    deleteItem->setAction("removeFile");
    mainLayout->addWidget(deleteItem);

    // thin divider between the file actions and the view toggles
    addSeparator(mainPage, mainLayout);

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
    addSeparator(mainPage, mainLayout);

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

    // Safety net: if the page still ends up taller than its items (e.g. a
    // future item count change), the surplus goes below the items instead
    // of being distributed between them by the QVBoxLayout spacing.
    convertLayout->addStretch(1);

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

void GridContextMenu::addSeparator(QWidget *page, QVBoxLayout *layout) {
    auto *separator = new QWidget(page);
    separator->setAccessibleName("HLineSeparator");
    separator->setFixedHeight(1);
    separator->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *sepLayout = new QVBoxLayout();
    sepLayout->setContentsMargins(11, 4, 11, 4);
    sepLayout->addWidget(separator);
    layout->addLayout(sepLayout);
}

void GridContextMenu::setSelectionInfo(const SelectionInfo &info) {
    convertItem->setEnabled(info.total() > 0 && info.allConvertible);
    renameItem->setEnabled(info.total() == 1);
    moveItem->setEnabled(info.total() > 0);
    trashItem->setEnabled(info.total() > 0);
    deleteItem->setEnabled(info.total() > 0);
}

// The stack holds pages of very different heights (11-row main page vs.
// 4-row convert page). QStackedLayout's sizeHint() is the max over *all*
// pages, except it zeroes out a page's contribution on axes where that
// page's size policy is Ignored - so the trick to make the popup track
// only the visible page is to mark every other page Ignored/Ignored and
// the visible one Preferred/Preferred before asking for a new sizeHint.
void GridContextMenu::setCurrentPage(int index) {
    for(int i = 0; i < stack->count(); ++i) {
        QWidget *page = stack->widget(i);
        bool current = (i == index);
        page->setSizePolicy(current ? QSizePolicy::Preferred : QSizePolicy::Ignored,
                             current ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    }

    stack->setCurrentIndex(index);

    // setSizePolicy() already calls updateGeometry(), but activate the
    // layouts explicitly so the new sizeHint is computed fresh rather than
    // reused from a cached value further up the chain.
    if(QLayout *pageLayout = stack->currentWidget()->layout())
        pageLayout->activate();
    stack->updateGeometry();
    layout()->activate();
    adjustSize();

    if(isVisible()) {
        // adjustSize() keeps the top-left corner fixed and grows/shrinks
        // toward bottom-right, so re-clamp in case switching to a taller
        // page (e.g. "Back" to the main page) pushed it off-screen.
        QRect geom = geometry();
        clampToScreen(geom);
        setGeometry(geom);
    }
}

void GridContextMenu::switchToMainPage() {
    setCurrentPage(0);
}

void GridContextMenu::switchToConvertPage() {
    setCurrentPage(1);
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

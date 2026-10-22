#include "copyoverlay.h"
#include "ui_copyoverlay.h"

CopyOverlay::CopyOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::CopyOverlay)
{
    ui->setupUi(this);
    hide();
    setFadeEnabled(true);

    ui->closeButton->setIconPath(":/res/icons/common/overlay/close-dim16.png");
    ui->headerIcon->setIconPath(":/res/icons/common/overlay/copy16.png");
    ui->headerLabel->setText(tr("Copy to..."));
    mode = OVERLAY_COPY;

    paths = settings->savedPaths();
    if(paths.count() < maxPathCount)
        createDefaultPaths();
    createPathWidgets();

    setAcceptKeyboardFocus(true);

    if(parent)
        setContainerSize(parent->size());

    readSettings();
    connect(settings, &Settings::settingsChanged, this, &CopyOverlay::readSettings);
}

CopyOverlay::~CopyOverlay() {
    delete ui;
}

void CopyOverlay::show() {
    updateShortcutLabels();
    OverlayWidget::show();
    setFocus();
}

void CopyOverlay::hide() {
    OverlayWidget::hide();
}

void CopyOverlay::setDialogMode(CopyOverlayMode _mode) {
    mode = _mode;
    if(mode == OVERLAY_COPY) {
        ui->headerIcon->setIconPath(":/res/icons/common/overlay/copy16.png");
        ui->headerLabel->setText(tr("Copy to..."));
    } else {
        ui->headerIcon->setIconPath(":/res/icons/common/overlay/move16.png");
        ui->headerLabel->setText(tr("Move to..."));
    }
}

CopyOverlayMode CopyOverlay::operationMode() {
    return mode;
}

void CopyOverlay::removePathWidgets() {
    for(int i = 0; i < pathWidgets.count(); i++) {
        QWidget *tmp = pathWidgets.at(i);
        ui->pathSelectorsLayout->removeWidget(tmp);
        delete tmp;
    }
    pathWidgets.clear();
}

void CopyOverlay::createPathWidgets() {
    removePathWidgets();
    int count = (paths.length() > maxPathCount) ? maxPathCount : paths.length();
    for(int i = 0; i < count; i++) {
        PathSelectorMenuItem *item = new PathSelectorMenuItem(this);
        item->setDirectory(paths.at(i));
        connect(item, &PathSelectorMenuItem::directorySelected, this, &CopyOverlay::requestFileOperation);
        pathWidgets.append(item);
        ui->pathSelectorsLayout->addWidget(item);
    }
    updateShortcutLabels();
}

// each folder shows the key bound to its sendToFolder action
void CopyOverlay::updateShortcutLabels() {
    for(int i = 0; i < pathWidgets.count(); i++) {
        const QString action = QStringLiteral("sendToFolder%1").arg(i + 1);
        pathWidgets.at(i)->setShortcutText(actionManager->shortcutForAction(MODE_COPYMOVE, action));
    }
}

void CopyOverlay::sendToFolder(int slot) {
    if(slot >= 0 && slot < pathWidgets.count())
        requestFileOperation(pathWidgets.at(slot)->directory());
}

void CopyOverlay::requestFileOperation(const QString& path) {
    if(mode == OVERLAY_COPY)
        emit copyRequested(path);
    else
        emit moveRequested(path);
}

void CopyOverlay::readSettings() {
    // don't interfere with the main panel
    if(settings->panelEnabled() && settings->panelPosition() == PanelPosition::PANEL_BOTTOM) {
        setPosition(FloatingWidgetPosition::TOPLEFT);
    } else {
        setPosition(FloatingWidgetPosition::BOTTOMLEFT);
    }
    update();
}

// for some reason, duplicate folders may appear in the configuration
// we remove duplicate directories
void CopyOverlay::saveSettings() {
    paths.clear();
    QStringList temp;
    for(int i = 0; i< pathWidgets.count(); i++) {
        QString path = pathWidgets.at(i)->path();
        if (!path.isEmpty()) {
            if (!temp.contains(path)) {
                temp << path;
                paths << pathWidgets.at(i)->directory();
            }
        }
    }
    settings->setSavedPaths(paths);
}

void CopyOverlay::createDefaultPaths() {
    QString home = QDir::homePath();
    if (paths.count() < 1 || paths.at(0).isEmpty() || paths.at(0)[0] == '@') {
        paths.clear();
        paths << home;
    }
    if (paths.count() == 1 && paths.at(0) == home) {
        QDir dir(home);
        for(const auto& mfi : dir.entryInfoList()) {
            if (paths.count() >= maxPathCount) {
                break;
            }
            if(mfi.isFile()) {
                continue;
            }
            else {
                if(mfi.fileName() == "."  
                || mfi.fileName() ==  ".."
                // hide directory
                || mfi.fileName()[0] ==  '.' 
                // windows system directory
                || mfi.fileName() ==  "3D Objects"
                || mfi.fileName() ==  "Contacts"
                || mfi.fileName() ==  "Favorites"
                || mfi.fileName() ==  "Links"
                || mfi.fileName() ==  "Saved Games"
                || mfi.fileName() ==  "Searches"
                ) {
                    continue;
                }
                QString qpath(home + "/" + mfi.fileName());
                QFileInfo qinfo(qpath);
                if (qinfo.permission(QFile::WriteUser | QFile::ReadGroup)) {
                    paths << qpath;
                }
            }
        }
    }
}

// block native tab-switching so we can use it in shortcuts
bool CopyOverlay::focusNextPrevChild(bool mode) {
    return false;
}

// Keys bound in the Copy / Move context (sendToFolder1..9: the digits by
// default) run first; every other key reaches the shortcuts as usual. A
// modified key without a binding of its own there uses its plain key's, as
// when the digits were hard-coded (Shift+1 picks the first folder too).
void CopyOverlay::keyPressEvent(QKeyEvent *event) {
    event->accept();
    QString action = actionManager->actionForShortcut(MODE_COPYMOVE, ShortcutBuilder::fromEvent(event));
    if(action.isEmpty())
        action = actionManager->actionForShortcut(MODE_COPYMOVE, actionManager->keyForNativeScancode(event->nativeScanCode()));
    if(action.isEmpty() || !actionManager->invokeAction(action))
        actionManager->processEvent(event);
}

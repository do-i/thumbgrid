#include "saveconfirmoverlay.h"
#include "constants.h"
#include "ui_saveconfirmoverlay.h"
#include "components/actionmanager/actionmanager.h"

namespace {
constexpr int kDimDelayMs = 2500;
constexpr qreal kDimmedOpacity = 0.45;
}

SaveConfirmOverlay::SaveConfirmOverlay(FloatingWidgetContainer *parent) :
    OverlayWidget(parent),
    ui(new Ui::SaveConfirmOverlay)
{
    ui->setupUi(this);
    connect(ui->saveButton,    &QPushButton::clicked, this, &SaveConfirmOverlay::saveClicked);
    connect(ui->saveAsButton,  &QPushButton::clicked, this, &SaveConfirmOverlay::saveAsClicked);
    connect(ui->discardButton, &QPushButton::clicked, this, &SaveConfirmOverlay::discardClicked);
    connect(ui->toggleButton,   &IconButton::clicked, this, [this]() { setExpanded(!expanded); });
    connect(ui->collapseButton, &IconButton::clicked, this, [this]() { setExpanded(false); });
    this->setFocusPolicy(Qt::NoFocus);
    ui->toggleButton->setIconPath(":res/icons/common/overlay/edit16.png");
    ui->collapseButton->setIconPath(":res/icons/common/overlay/collapse16.png");
    ui->collapseButton->setToolTip(tr("Fold back to the icon"));

    dimTimer.setSingleShot(true);
    dimTimer.setInterval(kDimDelayMs);
    connect(&dimTimer, &QTimer::timeout, this, &SaveConfirmOverlay::dim);
    dimAnimation = new QPropertyAnimation(this, "opacity", this);
    dimAnimation->setDuration(tg::timing::kOverlayFadeMs);
    dimAnimation->setEasingCurve(QEasingCurve::OutQuad);

    // hug the corner: as little of the image under it as possible
    setHorizontalMargin(16);
    setVerticalMargin(16);
    readSettings();
    connect(settings, &Settings::settingsChanged, this, &SaveConfirmOverlay::readSettings);
    setBusy(false);
    setExpanded(false);

    if(parent)
        setContainerSize(parent->size());

    this->hide();
}

void SaveConfirmOverlay::readSettings() {
    // don't interfere with the main panel
    if(settings->panelEnabled() && settings->panelPosition() == PanelPosition::PANEL_BOTTOM) {
        setPosition(FloatingWidgetPosition::TOPRIGHT);
    } else {
        setPosition(FloatingWidgetPosition::BOTTOMRIGHT);
    }
    updateToolTips();
    update();
}

void SaveConfirmOverlay::updateToolTips() {
    auto withShortcut = [](const QString &text, const QString &action) {
        const QStringList keys = actionManager ? actionManager->shortcutsForAction(action) : QStringList();
        return keys.isEmpty() ? text : text + "  (" + keys.first() + ")";
    };
    ui->saveButton->setToolTip(withShortcut(tr("Overwrite the file with the edits"), "save"));
    ui->saveAsButton->setToolTip(withShortcut(tr("Save the edits to a new file"), "saveAs"));
    ui->discardButton->setToolTip(withShortcut(tr("Undo every edit: back to the file on disk"), "discardEdits"));
}

// every show is a fresh edit (or a newly shown edited image): full strength, then fade back
void SaveConfirmOverlay::show() {
    dimAnimation->stop();
    dimmed = false;
    OverlayWidget::show();
    scheduleDim();
}

// next time it appears (another image, a new edit after saving) it starts as the icon again
void SaveConfirmOverlay::hide() {
    dimTimer.stop();
    dimAnimation->stop();
    dimmed = false;
    OverlayWidget::hide();
    setExpanded(false);
}

bool SaveConfirmOverlay::isExpanded() const {
    return expanded;
}

void SaveConfirmOverlay::setExpanded(bool mode) {
    expanded = mode;
    ui->saveButton->setVisible(expanded);
    ui->saveAsButton->setVisible(expanded);
    ui->discardButton->setVisible(expanded);
    ui->collapseButton->setVisible(expanded);
    ui->toggleButton->setToolTip(expanded ? tr("Hide save options")
                                          : tr("Unsaved edits: click for save options"));
    relayout();
    // toggling is interaction: full strength, then fade again once left alone
    if(isVisible()) {
        dimTimer.stop();
        wake();
        scheduleDim();
    }
}

// the size changes with the mode: keep the pill anchored to its corner
void SaveConfirmOverlay::relayout() {
    if(layout())
        layout()->activate();
    recalculateGeometry();
}

void SaveConfirmOverlay::setBusy(bool mode) {
    busy = mode;
    // no caption while idle: the pencil says it all; only saving gets a word
    ui->label->setVisible(busy);
    ui->saveButton->setEnabled(!busy);
    ui->saveAsButton->setEnabled(!busy);
    ui->discardButton->setEnabled(!busy);
    if(busy) {
        dimTimer.stop();
        wake();
    } else {
        scheduleDim();
    }
    relayout();
}

bool SaveConfirmOverlay::isDimmed() const {
    return dimmed;
}

void SaveConfirmOverlay::scheduleDim() {
    if(busy || underMouse() || isHidden())
        return;
    dimTimer.start();
}

void SaveConfirmOverlay::dim() {
    if(busy || underMouse() || isHidden())
        return;
    dimmed = true;
    dimAnimation->stop();
    dimAnimation->setStartValue(opacity());
    dimAnimation->setEndValue(kDimmedOpacity);
    dimAnimation->start();
}

void SaveConfirmOverlay::wake() {
    dimmed = false;
    dimAnimation->stop();
    dimAnimation->setStartValue(opacity());
    dimAnimation->setEndValue(1.0);
    dimAnimation->start();
}

void SaveConfirmOverlay::enterEvent(QEnterEvent *event) {
    dimTimer.stop();
    wake();
    OverlayWidget::enterEvent(event);
}

void SaveConfirmOverlay::leaveEvent(QEvent *event) {
    OverlayWidget::leaveEvent(event);
    scheduleDim();
}

SaveConfirmOverlay::~SaveConfirmOverlay()
{
    delete ui;
}

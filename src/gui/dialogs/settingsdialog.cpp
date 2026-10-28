#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "appversion.h"
#include <QSignalBlocker>
#include <QToolButton>
#include <QDir>
#include "gui/dialogs/custommessagebox.h"
#include "gui/customwidgets/iconbutton.h"
#include "gui/customwidgets/scriptrowwidget.h"
#include <QHBoxLayout>
#include <QStyle>
#include <QFrame>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QStandardItemModel>
#include <functional>
#include "gui/customwidgets/keysequenceedit.h"
#include "utils/imagelib.h"

namespace {
// marks the "Custom" entry apart from a real preset row with the same id
constexpr int kCustomPresetEntryRole = Qt::UserRole + 1;

// all contexts share one table, so each cell carries its context next to the action id
constexpr int kShortcutContextRole = Qt::UserRole + 1;
// context label on the icon cell, for the search box
constexpr int kShortcutContextLabelRole = Qt::UserRole + 2;

enum ShortcutColumn {
    ShortcutColumnContext = 0,
    ShortcutColumnAction,
    ShortcutColumnKey,
    ShortcutColumnCount,
    ShortcutColumnEnabled,
    ShortcutColumnTotal
};

// icon-only cell: sort by context order, then by action
class ShortcutContextItem : public QTableWidgetItem
{
public:
    ShortcutContextItem(int order, QString actionLabel)
        : QTableWidgetItem(), mOrder(order), mActionLabel(std::move(actionLabel)) {}

    bool operator<(const QTableWidgetItem &other) const override {
        const auto *o = dynamic_cast<const ShortcutContextItem *>(&other);
        if(!o)
            return QTableWidgetItem::operator<(other);
        if(mOrder != o->mOrder)
            return mOrder < o->mOrder;
        return mActionLabel.compare(o->mActionLabel, Qt::CaseInsensitive) < 0;
    }

private:
    int mOrder;
    QString mActionLabel;
};

// QSS-styled ::item ignores TextAlignmentRole, so force alignment via the option;
// elide left so the filename survives
class RightAlignedPathDelegate : public QStyledItemDelegate
{
public:
    explicit RightAlignedPathDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
        option->textElideMode = Qt::ElideLeft;
    }
};

class CenteredCheckBoxDelegate : public QStyledItemDelegate
{
public:
    explicit CenteredCheckBoxDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if(!index.data(Qt::CheckStateRole).isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        itemOption.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        itemOption.text.clear();

        const QWidget *widget = option.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, widget);

        QStyleOptionButton checkOption;
        checkOption.state = option.state & (QStyle::State_Enabled | QStyle::State_Active | QStyle::State_MouseOver);
        const Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        if(state == Qt::Checked)
            checkOption.state |= QStyle::State_On;
        else if(state == Qt::PartiallyChecked)
            checkOption.state |= QStyle::State_NoChange;
        else
            checkOption.state |= QStyle::State_Off;

        const QSize checkSize(
            style->pixelMetric(QStyle::PM_IndicatorWidth, &checkOption, widget),
            style->pixelMetric(QStyle::PM_IndicatorHeight, &checkOption, widget));
        checkOption.rect = QStyle::alignedRect(option.direction, Qt::AlignCenter, checkSize, option.rect);
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkOption, painter, widget);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override {
        if(!index.data(Qt::CheckStateRole).isValid() || !(index.flags() & Qt::ItemIsUserCheckable))
            return QStyledItemDelegate::editorEvent(event, model, option, index);

        // Space is handled for the whole row by SettingsDialog::eventFilter
        if(event->type() != QEvent::MouseButtonRelease)
            return false;
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if(mouseEvent->button() != Qt::LeftButton || !option.rect.contains(mouseEvent->pos()))
            return false;

        const Qt::CheckState state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        return model->setData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    }
};
}

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("Preferences — ") + qApp->applicationName());

    setupPreferencePages();
    setupShortcutsPage();
    setupAboutPage();
    setupThemePage();
    setupFeatureToggles();
    setupScalingEngineControls();
    setupSidebar();
    setupMiscControls();

    connect(this, &SettingsDialog::settingsChanged, settings, &Settings::sendChangeNotification);
    readSettings();

    adjustSizeToContents();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupAboutPage() {
    ui->aboutAppTextBrowser->viewport()->setAutoFillBackground(false);
    // dev builds reveal the full git describe string on hover
    ui->versionLabel->setText(appVersionShort);
    // info button pops up the full git describe string
    auto *versionInfoButton = new QToolButton(this);
    versionInfoButton->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
    versionInfoButton->setAutoRaise(true);
    versionInfoButton->setCursor(Qt::PointingHandCursor);
    versionInfoButton->setToolTip(tr("Show full version"));
    versionInfoButton->setFixedSize(18, 18);
    versionInfoButton->setIconSize(QSize(12, 12));
    ui->horizontalLayout_41->insertWidget(
        ui->horizontalLayout_41->indexOf(ui->versionLabel) + 1, versionInfoButton);
    connect(versionInfoButton, &QToolButton::clicked, this, [this]() {
        CustomMessageBox::message(this, tr("Version"),
            tr("<b>thumbgrid %1</b><br><br>Build: %2")
                .arg(appVersionShort, appVersionFull));
    });
    ui->qtVersionLabel->setText(qVersion());
    ui->appIconLabel->setPixmap(QIcon(":/res/icons/common/logo/app/22.png").pixmap(22,22));
    ui->qtIconLabel->setPixmap(QIcon(":/res/icons/common/logo/3rdparty/qt22.png").pixmap(22,16));
}
//------------------------------------------------------------------------------
void SettingsDialog::setupThemePage() {
    // combobox used as a menu button
    connect(ui->themeSelectorComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this](int index) {
        // leaving custom: stash the palette so previewing a preset keeps the custom colors
        if(mPrevThemeIndex == 5 && index != 5) {
            mCustomColors = collectColorScheme();
            mCustomColors.tid = COLORS_CUSTOM;
        }
        switch(index) {
            case 0: setColorScheme(ThemeStore::colorScheme(COLORS_BLACK));        break;
            case 1: setColorScheme(ThemeStore::colorScheme(COLORS_DARK));         break;
            case 2: setColorScheme(ThemeStore::colorScheme(COLORS_DARKBLUE));     break;
            case 3: setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT));        break;
            case 4: setColorScheme(ThemeStore::colorScheme(COLORS_LIGHT_YELLOW)); break;
            case 5: setColorScheme(mCustomColors);                               break;
            case 6: setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));      break;
        }
        updateThemeControls(index);
        mPrevThemeIndex = index;
    });

    // turns the locked System palette into an editable custom copy
    connect(ui->modifySystemSchemeLabel, &ClickableLabel::clicked, [this]() {
        ColorScheme custom = ThemeStore::colorScheme(COLORS_SYSTEM);
        custom.tid = COLORS_CUSTOM;
        mCustomColors = custom;
        setColorScheme(custom);
        updateThemeControls(ui->themeSelectorComboBox->currentIndex());
        mPrevThemeIndex = ui->themeSelectorComboBox->currentIndex();
    });

    // App-wide
    ui->colorSelectorAccent->setDescription(tr("Accent"));
    ui->colorSelectorText->setDescription(tr("Text"));
    ui->colorSelectorIcons->setDescription(tr("Icons"));
    ui->colorSelectorBackground->setDescription(tr("Window background"));
    ui->colorSelectorWidget->setDescription(tr("Control background"));
    ui->colorSelectorWidgetBorder->setDescription(tr("Control border"));
    ui->colorSelectorScrollbar->setDescription(tr("Scrollbar"));
    // Grid
    ui->colorSelectorFolderview->setDescription(tr("Grid background"));
    ui->colorSelectorFolderviewPanel->setDescription(tr("Grid top bar"));
    ui->folderViewCellBackgroundColorSelector->setDescription(tr("Thumbnail cell"));
    ui->folderViewLabelBackgroundColorSelector->setDescription(tr("Filename label"));
    ui->folderViewSelectedLabelBackgroundColorSelector->setDescription(tr("Selected filename label"));
    ui->folderViewSelectionColorSelector->setDescription(tr("Selection highlight"));
    ui->folderViewParentIconColorSelector->setDescription(tr("Folder-icon tint"));
    // Content view
    ui->colorSelectorFullscreen->setDescription(tr("Fullscreen background"));
    ui->colorSelectorOverlay->setDescription(tr("Overlay background"));
    ui->colorSelectorOverlayText->setDescription(tr("Overlay text"));

    // every color button is a theme color: OK marks the theme custom, Apply also re-themes live
    auto switchToCustom = [this](QColor) { markThemeCustom(); };
    auto previewColor = [this](QColor) {
        markThemeCustom();
        applyColorSchemePreview();
    };
    for(auto *selector : findChildren<ColorSelectorButton *>()) {
        connect(selector, &ColorSelectorButton::colorChanged, this, switchToCustom);
        connect(selector, &ColorSelectorButton::colorApplied, this, previewColor);
    }
}
//------------------------------------------------------------------------------
// disable or hide controls for features this build lacks
// Beta comparison of the Qt and OpenCV scaling engines. The engine row sits above the
// filter row because it decides which filters are listed.
void SettingsDialog::setupScalingEngineControls() {
    for(int i = 0; i < ui->scalingQualityComboBox->count(); i++)
        mScalingFilterNames << ui->scalingQualityComboBox->itemText(i);
    auto *row = new QHBoxLayout();
    auto *label = new QLabel(tr("Scaling engine:"));
    mScalingEngineComboBox = new QComboBox();
    mScalingEngineComboBox->setObjectName("scalingEngineComboBox");
    mScalingEngineComboBox->addItem(tr("Qt"));
    mScalingEngineComboBox->addItem(tr("OpenCV (legacy)"));
    mScalingEngineComboBox->setToolTip(tr("Being compared before OpenCV is removed. The toggleScalingEngine "
                                          "shortcut action flips between them on the shown image."));
    const int labelWidth = qMax(label->sizeHint().width(), ui->scalingQualityLabel->sizeHint().width());
    label->setFixedWidth(labelWidth);
    ui->scalingQualityLabel->setFixedWidth(labelWidth);
    const int comboWidth = ui->scalingQualityComboBox->sizeHint().width();
    mScalingEngineComboBox->setFixedWidth(comboWidth);
    ui->scalingQualityComboBox->setFixedWidth(comboWidth);
    row->addWidget(label);
    row->addWidget(mScalingEngineComboBox);
    row->addStretch(1);
    mScalingSharpenCheckBox = new QCheckBox(tr("Sharpen images shrunk below 50% (Qt engine)"));
    mScalingSharpenCheckBox->setObjectName("scalingSharpenCheckBox");
    const int filterRow = ui->verticalLayout_24->indexOf(ui->horizontalLayout_5);
    ui->verticalLayout_24->insertLayout(filterRow, row);
    ui->verticalLayout_24->insertWidget(filterRow + 2, mScalingSharpenCheckBox);
#ifndef USE_OPENCV
    label->hide();
    mScalingEngineComboBox->hide();
#endif
    connect(mScalingEngineComboBox, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateScalingEngineControls);
    connect(ui->scalingQualityComboBox, &QComboBox::activated, this, &SettingsDialog::onScalingFilterActivated);
}

void SettingsDialog::updateScalingEngineControls() {
    const bool legacy = mScalingEngineComboBox->currentIndex() == 1;
    mScalingSharpenCheckBox->setEnabled(!legacy);
    const QSignalBlocker blocker(ui->scalingQualityComboBox);
    ui->scalingQualityComboBox->clear();
    if(legacy) {
        ui->scalingQualityComboBox->addItems(mScalingFilterNames);
        ui->scalingQualityComboBox->setCurrentIndex(mScalingFilterChoice);
    } else {
        ui->scalingQualityComboBox->addItems({mScalingFilterNames.value(QI_FILTER_NEAREST), tr("Smooth")});
        ui->scalingQualityComboBox->setCurrentIndex(mScalingFilterChoice == QI_FILTER_NEAREST ? 0 : 1);
    }
}

void SettingsDialog::onScalingFilterActivated(int index) {
    if(mScalingEngineComboBox->currentIndex() == 1)
        mScalingFilterChoice = static_cast<ScalingFilter>(index);
    else if(index == 0)
        mScalingFilterChoice = QI_FILTER_NEAREST;
    else if(mScalingFilterChoice == QI_FILTER_NEAREST)
        mScalingFilterChoice = mScalingFilterNames.size() > QI_FILTER_CV_CUBIC ? QI_FILTER_CV_CUBIC : QI_FILTER_BILINEAR;
}

void SettingsDialog::setupFeatureToggles() {
#ifndef USE_KDE_BLUR
    ui->blurBackgroundCheckBox->setEnabled(false);
#endif

#ifndef USE_MPV
    ui->videoPlaybackGroup->setEnabled(false);
#endif

#ifdef USE_OPENCV
    ui->scalingQualityComboBox->addItem("Bilinear+sharpen (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic (OpenCV)");
    ui->scalingQualityComboBox->addItem("Bicubic+sharpen (OpenCV)");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    ui->memoryLimitSpinBox->setEnabled(false);
    ui->memoryLimitLabel->setEnabled(false);
#endif

    if(!settings->supportedFormats().contains("jxl"))
        ui->animatedJxlCheckBox->hide();
}
//------------------------------------------------------------------------------
void SettingsDialog::setupMiscControls() {
    // setup radioBtn groups
    fitModeGrp.addButton(ui->fitModeWindow);
    fitModeGrp.addButton(ui->fitModeWidth);
    fitModeGrp.addButton(ui->fitMode1to1);
    fitModeGrp.addButton(ui->fitModeWindowStretch);
    folderEndGrp.addButton(ui->folderEndSwitchFolder);
    folderEndGrp.addButton(ui->folderEndNoAction);
    folderEndGrp.addButton(ui->folderEndLoop);
    zoomIndGrp.addButton(ui->zoomIndicatorAuto);
    zoomIndGrp.addButton(ui->zoomIndicatorOff);
    zoomIndGrp.addButton(ui->zoomIndicatorOn);

    // readable language names
    langs.insert("de_DE", "Deutsch");
    langs.insert("en_US", "English");
    langs.insert("es_ES", "Español");
    langs.insert("fr_FR", "Français");
    langs.insert("ja_JP", "日本語");
    langs.insert("tr_TR", "Türkçe");
    langs.insert("uk_UA", "Українська");
    langs.insert("zh_CN", "简体中文");
    // fill langs combobox, sorted by locale
    ui->langComboBox->addItems(langs.values());
    // insert system language entry manually at the beginning
    langs.insert("system", "System language");
    ui->langComboBox->insertItem(0, "System language");

    // like CustomMessageBox: accented default OK, pointing-hand cursors
    ui->OK->setDefault(true);
    ui->OK->setCursor(Qt::PointingHandCursor);
    ui->Cancel->setCursor(Qt::PointingHandCursor);
    ui->applyButton->setCursor(Qt::PointingHandCursor);
}
//------------------------------------------------------------------------------
SettingsDialog::~SettingsDialog() {
    delete ui;
}
//------------------------------------------------------------------------------
QWidget* SettingsDialog::makeSettingsPage(const QString &title, QVBoxLayout **contentLayout) {
    QWidget *page = new QWidget(ui->stackedWidget);
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);

    QScrollArea *scrollArea = new QScrollArea(page);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    QWidget *contents = new QWidget(scrollArea);
    contents->setAccessibleName("SPageContents");
    QVBoxLayout *layout = new QVBoxLayout(contents);
    layout->setSpacing(9);
    layout->setContentsMargins(18, 9, 18, 9);

    QLabel *header = new QLabel(title, contents);
    header->setAccessibleName("SHeaderText");
    layout->addWidget(header);

    QWidget *line = new QWidget(contents);
    line->setAccessibleName("SHeaderLine");
    layout->addWidget(line);
    layout->addSpacing(6);

    *contentLayout = layout;
    scrollArea->setWidget(contents);
    outer->addWidget(scrollArea);
    return page;
}
//------------------------------------------------------------------------------
QWidget* SettingsDialog::makeSettingsGroup(const QString &title) {
    QWidget *group = new QWidget(this);
    group->setAccessibleName("SGroup");
    QVBoxLayout *layout = new QVBoxLayout(group);
    layout->setSpacing(7);
    layout->setContentsMargins(13, 10, 13, 10);

    if(!title.isEmpty()) {
        QLabel *label = new QLabel(title, group);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        layout->addWidget(label);
    }
    return group;
}
//------------------------------------------------------------------------------
void SettingsDialog::setupPreferencePages() {
    ui->label_29->setText(tr("Shortcuts"));
    ui->label_43->setText(tr("Every script is also listed on the Shortcuts page, where you can assign it a key."));
    ui->startInFolderViewCheckBox->setText(tr("Start in grid view by default"));
    ui->folderViewTopBarCheckBox->setText(tr("Show grid top bar"));
    ui->folderViewFontSizeLabel->setText(tr("Grid filename font size:"));
    ui->label_30->setText(tr("Grid navigation"));

    QVBoxLayout *gridLayout = nullptr;
    QWidget *gridPage = makeSettingsPage(tr("Grid"), &gridLayout);
    QWidget *gridDisplayGroup = makeSettingsGroup(tr("Grid display"));
    auto *gridDisplayLayout = qobject_cast<QVBoxLayout*>(gridDisplayGroup->layout());
    gridDisplayLayout->addWidget(ui->folderViewTopBarCheckBox);
    QHBoxLayout *fontLayout = new QHBoxLayout();
    fontLayout->setContentsMargins(0, 0, 0, 0);
    fontLayout->addWidget(ui->folderViewFontSizeLabel);
    fontLayout->addWidget(ui->folderViewFontSizeSpinBox);
    fontLayout->addStretch();
    gridDisplayLayout->addLayout(fontLayout);
    gridLayout->addWidget(gridDisplayGroup);
    gridLayout->addSpacing(12);
    gridLayout->addWidget(ui->folderNavGroup);
    gridLayout->addStretch();

    QVBoxLayout *documentLayout = nullptr;
    QWidget *documentPage = makeSettingsPage(tr("Document"), &documentLayout);
    documentLayout->addWidget(ui->displayGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->zoomGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->scalingGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->videoPlaybackGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->slideshowGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->thumbnailPanelGroup);
    documentLayout->addSpacing(12);
    documentLayout->addWidget(ui->widget_12);
    documentLayout->addStretch();

    // window opacity/blur belong on the General page
    ui->verticalLayout_5->insertWidget(
        ui->verticalLayout_5->indexOf(ui->UIOptionsGroup) + 1, ui->windowTweaksGroup);

    QStackedWidget *stack = ui->stackedWidget;
    const QList<QWidget*> oldPages = {ui->General, ui->View, ui->Theme, ui->Controls,
                                      ui->Scripts, ui->Advanced, ui->About};
    for(QWidget *page : oldPages)
        stack->removeWidget(page);

    stack->addWidget(ui->General);
    stack->addWidget(ui->Theme);
    stack->addWidget(ui->Controls);
    stack->addWidget(gridPage);
    stack->addWidget(documentPage);
    stack->addWidget(ui->Scripts);
    stack->addWidget(ui->Advanced);
    stack->addWidget(setupStoredDataPage());
    stack->addWidget(ui->About);

    // store sizes can be slow (thumbnail walk), so refresh when the page opens
    connect(stack, &QStackedWidget::currentChanged, this, [this](int) {
        if(ui->stackedWidget->currentWidget() == mStoredDataPage)
            refreshStoredDataDetails();
    });
}
//------------------------------------------------------------------------------
QWidget* SettingsDialog::setupStoredDataPage() {
    QVBoxLayout *layout = nullptr;
    mStoredDataPage = makeSettingsPage(tr("Stored data"), &layout);
    QWidget *group = makeSettingsGroup(tr("Data stored on this computer"));
    auto *groupLayout = qobject_cast<QVBoxLayout*>(group->layout());

    QLabel *hint = new QLabel(tr("Everything the app remembers about your files and folders. "
                                 "Delete a row with its ✕ button, or check rows to delete them "
                                 "together — right away or on every exit."), group);
    hint->setWordWrap(true);
    groupLayout->addWidget(hint);

    mStoredDataSelectAll = new QCheckBox(tr("Select all"), group);
    mStoredDataSelectAll->setObjectName("storedDataSelectAll");
    groupLayout->addWidget(mStoredDataSelectAll);

    mStoredDataTable = new QTreeWidget(group);
    mStoredDataTable->setObjectName("storedDataTable");
    mStoredDataTable->setColumnCount(4);
    mStoredDataTable->setHeaderLabels({tr("Store"), tr("Path"), tr("Size"), QString()});
    mStoredDataTable->setRootIsDecorated(false);
    mStoredDataTable->setUniformRowHeights(true);
    mStoredDataTable->setAllColumnsShowFocus(true);
    mStoredDataTable->setSelectionMode(QAbstractItemView::NoSelection);
    mStoredDataTable->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    mStoredDataTable->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    mStoredDataTable->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    mStoredDataTable->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    mStoredDataTable->header()->setStretchLastSection(false);
    mStoredDataTable->setItemDelegateForColumn(1, new RightAlignedPathDelegate(mStoredDataTable));

    mStores = StoredData::stores();
    for(int i = 0; i < mStores.count(); i++) {
        const StoredDataStore &store = mStores.at(i);
        auto *item = new QTreeWidgetItem(mStoredDataTable);
        item->setText(0, store.title);
        item->setToolTip(0, store.description);
        const QString path = QDir::toNativeSeparators(QDir::cleanPath(store.path));
        item->setText(1, path);
        item->setToolTip(1, path);
        item->setCheckState(0, Qt::Unchecked);
        auto *removeButton = new QToolButton(mStoredDataTable);
        removeButton->setText(QStringLiteral("✕"));
        removeButton->setAutoRaise(true);
        removeButton->setCursor(Qt::PointingHandCursor);
        removeButton->setToolTip(tr("Delete %1 now").arg(store.title));
        connect(removeButton, &QToolButton::clicked, this, [this, i] { deleteStoredData({i}); });
        mStoredDataTable->setItemWidget(item, 3, removeButton);
    }
    // 5 fixed rows - size the view to them instead of scrolling inside the page
    mStoredDataTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mStoredDataTable->setMinimumHeight(
        mStoredDataTable->sizeHintForRow(0) * (mStoredDataTable->topLevelItemCount() + 2));
    groupLayout->addWidget(mStoredDataTable);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    mDeleteSelectedButton = new QPushButton(tr("Delete selected now"), group);
    mDeleteSelectedButton->setObjectName("storedDataDeleteSelected");
    buttonRow->addWidget(mDeleteSelectedButton);
    buttonRow->addStretch();
    groupLayout->addLayout(buttonRow);

    mClearOnExitBox = new QCheckBox(tr("Delete selected every time the app exits"), group);
    mClearOnExitBox->setObjectName("storedDataClearOnExit");
    groupLayout->addWidget(mClearOnExitBox);

    layout->addWidget(group);
    layout->addStretch();

    connect(mStoredDataTable, &QTreeWidget::itemChanged, this, [this] {
        if(mStoredDataUpdating)
            return;
        updateStoredDataControls();
        persistClearOnExit();
    });
    connect(mStoredDataSelectAll, &QCheckBox::clicked, this, [this](bool checked) {
        mStoredDataUpdating = true;
        for(int i = 0; i < mStoredDataTable->topLevelItemCount(); i++)
            mStoredDataTable->topLevelItem(i)->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        mStoredDataUpdating = false;
        updateStoredDataControls();
        persistClearOnExit();
    });
    connect(mDeleteSelectedButton, &QPushButton::clicked, this, [this] {
        deleteStoredData(checkedStoredDataRows());
    });
    connect(mClearOnExitBox, &QCheckBox::toggled, this, [this] {
        if(!mStoredDataUpdating)
            persistClearOnExit();
    });

    // The on-exit clear list doubles as the persisted row selection.
    const QStringList exitIds = settings->clearOnExitStores();
    if(!exitIds.isEmpty()) {
        mStoredDataUpdating = true;
        mClearOnExitBox->setChecked(true);
        for(int i = 0; i < mStores.count(); i++)
            if(exitIds.contains(mStores.at(i).id))
                mStoredDataTable->topLevelItem(i)->setCheckState(0, Qt::Checked);
        mStoredDataUpdating = false;
    }
    updateStoredDataControls();
    return mStoredDataPage;
}

void SettingsDialog::refreshStoredDataDetails() {
    for(int i = 0; i < mStores.count(); i++)
        mStoredDataTable->topLevelItem(i)->setText(2, mStores.at(i).detail());
}

QList<int> SettingsDialog::checkedStoredDataRows() const {
    QList<int> rows;
    for(int i = 0; i < mStoredDataTable->topLevelItemCount(); i++)
        if(mStoredDataTable->topLevelItem(i)->checkState(0) == Qt::Checked)
            rows.append(i);
    return rows;
}

void SettingsDialog::deleteStoredData(const QList<int> &rows) {
    if(rows.isEmpty())
        return;
    const QString text = rows.count() == 1
        ? tr("Delete %1? This cannot be undone.").arg(mStores.at(rows.first()).title.toLower())
        : tr("Delete %n selected data store(s)? This cannot be undone.", nullptr, rows.count());
    if(!CustomMessageBox::confirm(this, tr("Delete stored data"), text, true))
        return;
    for(int row : rows)
        mStores.at(row).clear();
    refreshStoredDataDetails();
}

void SettingsDialog::persistClearOnExit() {
    QStringList ids;
    if(mClearOnExitBox->isChecked())
        for(int row : checkedStoredDataRows())
            ids.append(mStores.at(row).id);
    settings->setClearOnExitStores(ids);
}

void SettingsDialog::updateStoredDataControls() {
    const int checked = checkedStoredDataRows().count();
    mDeleteSelectedButton->setEnabled(checked > 0);
    mStoredDataSelectAll->blockSignals(true);
    if(!checked)
        mStoredDataSelectAll->setCheckState(Qt::Unchecked);
    else if(checked == mStoredDataTable->topLevelItemCount())
        mStoredDataSelectAll->setCheckState(Qt::Checked);
    else
        mStoredDataSelectAll->setCheckState(Qt::PartiallyChecked);
    mStoredDataSelectAll->blockSignals(false);
}
//------------------------------------------------------------------------------
void SettingsDialog::setupShortcutsPage() {
    // "+ New shortcut" is the single entry point
    ui->pushButton_2->hide();
    ui->pushButton_8->hide();
    ui->pushButton_4->hide();

    ui->newShortcutButton->setText(tr("+ New shortcut"));
    ui->newShortcutButton->setCursor(Qt::PointingHandCursor);
    connect(ui->newShortcutButton, &QPushButton::clicked, this, &SettingsDialog::addShortcut);

    // switching presets replaces the whole mapping, so it is confirmed and applied immediately
    QLabel *presetLabel = new QLabel(tr("Shortcut preset:"), ui->Controls);
    mShortcutPresetComboBox = new QComboBox(ui->Controls);
    mShortcutPresetComboBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QHBoxLayout *presetRow = new QHBoxLayout();
    presetRow->addWidget(presetLabel);
    presetRow->addWidget(mShortcutPresetComboBox);
    presetRow->addStretch(1);
    ui->verticalLayout_33->insertLayout(0, presetRow);

    connect(mShortcutPresetComboBox, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        // re-picking the preset's own row (not "Custom") means discard edits, so it is not a no-op
        if(mShortcutPresetComboBox->itemData(index, kCustomPresetEntryRole).toBool())
            return;
        const QString id = mShortcutPresetComboBox->itemData(index).toString();
        if(id.isEmpty())
            return;
        if(id == ActionManager::selectedPreset() && !settings->shortcutsModified())
            return; // already exactly this preset with nothing to discard
        const bool proceed = CustomMessageBox::confirm(this,
            tr("Switch shortcut preset"),
            tr("Switching to \"%1\" replaces all current keyboard/mouse shortcuts "
               "with that preset's bindings. This cannot be undone from this dialog. Continue?")
                .arg(mShortcutPresetComboBox->itemText(index)), true);
        if(!proceed) {
            refreshShortcutPresetCombo(); // revert the displayed selection
            return;
        }
        actionManager->applyPreset(id);
        readShortcuts(); // reloads the draft/table and relabels the combo (modified -> false)
    });
    refreshShortcutPresetCombo();

    // search also matches the context label, so "grid" narrows to one context
    mShortcutSearchEdit = new QLineEdit(ui->Controls);
    mShortcutSearchEdit->setPlaceholderText(tr("Search actions, keys and contexts"));
    mShortcutSearchEdit->setClearButtonEnabled(true);

    ui->horizontalLayout_2->insertWidget(0, mShortcutSearchEdit, 1);

    disconnect(ui->shortcutsTableWidget, nullptr, this, nullptr);
    ui->shortcutsTableWidget->setColumnCount(ShortcutColumnTotal);
    ui->shortcutsTableWidget->setHorizontalHeaderLabels(
        {tr("Context"), tr("Action"), tr("Key"), tr("Count"), tr("Enabled")});
    ui->shortcutsTableWidget->setIconSize(QSize(16, 16));
    ui->shortcutsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->shortcutsTableWidget->setItemDelegateForColumn(ShortcutColumnEnabled,
                                                       new CenteredCheckBoxDelegate(ui->shortcutsTableWidget));
    ui->shortcutsTableWidget->setSortingEnabled(true);
    ui->shortcutsTableWidget->horizontalHeader()->setSectionsClickable(true);
    ui->shortcutsTableWidget->horizontalHeader()->setSortIndicatorShown(true);
    connect(ui->shortcutsTableWidget->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this](int column, Qt::SortOrder order) {
        if(mUpdatingShortcutsTable)
            return;
        settings->setShortcutsSortColumn(column);
        settings->setShortcutsSortOrder(order);
    });
    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(ShortcutColumnContext, QHeaderView::ResizeToContents);
    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(ShortcutColumnAction, QHeaderView::Stretch);
    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(ShortcutColumnKey, QHeaderView::Stretch);
    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(ShortcutColumnCount, QHeaderView::ResizeToContents);
    ui->shortcutsTableWidget->horizontalHeader()->setSectionResizeMode(ShortcutColumnEnabled, QHeaderView::ResizeToContents);

    connect(mShortcutSearchEdit, &QLineEdit::textChanged,
            this, [this]() { updateShortcutsFilter(); });
    connect(ui->shortcutsTableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if(mUpdatingShortcutsTable || !item || item->column() != ShortcutColumnEnabled)
            return;
        const QString action = item->data(Qt::UserRole).toString();
        const ViewMode context = static_cast<ViewMode>(item->data(kShortcutContextRole).toInt());
        const int column = ui->shortcutsTableWidget->currentColumn();
        setShortcutEnabled(context, action, item->checkState() == Qt::Checked);
        // keep the toggled row current so Space can switch it back
        updateShortcutsTable();
        setCurrentShortcutRow(context, action, column >= 0 ? column : ShortcutColumnEnabled);
    });
    ui->shortcutsTableWidget->installEventFilter(this);
    connect(ui->shortcutsTableWidget, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int column) {
        if(column == ShortcutColumnKey)
            openShortcutDetails(row);
    });
    connect(ui->shortcutsTableWidget, &QTableWidget::cellClicked,
            this, [this](int row, int column) {
        if(column == ShortcutColumnKey)
            openShortcutDetails(row);
    });
    // retargeting a binding to another context is a row command
    ui->shortcutsTableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->shortcutsTableWidget, &QWidget::customContextMenuRequested,
            this, &SettingsDialog::showShortcutRowMenu);

    // repaint context icons in place when the theme's icon color changes
    connect(settings, &Settings::settingsChanged, this, [this]() {
        mShortcutContextIcons.clear();
        QSignalBlocker blocker(ui->shortcutsTableWidget);
        for(int row = 0; row < ui->shortcutsTableWidget->rowCount(); row++) {
            QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, ShortcutColumnContext);
            if(item)
                item->setIcon(shortcutContextIcon(shortcutContextAtRow(row)));
        }
    });
}
//------------------------------------------------------------------------------
void SettingsDialog::refreshShortcutPresetCombo() {
    if(!mShortcutPresetComboBox)
        return;
    const QString current = ActionManager::selectedPreset();
    const QList<PresetInfo> presets = ActionManager::availablePresets();

    QSignalBlocker blocker(mShortcutPresetComboBox);
    mShortcutPresetComboBox->clear();
    int currentPresetIndex = -1;
    for(const PresetInfo &p : presets) {
        mShortcutPresetComboBox->addItem(p.name, p.id);
        if(p.id == current)
            currentPresetIndex = mShortcutPresetComboBox->count() - 1;
    }

    if(currentPresetIndex < 0) {
        // the stored preset is unavailable here (other OS / pruned); show it, mapping unchanged
        mShortcutPresetComboBox->addItem(tr("%1 (unavailable)").arg(current), current);
        mShortcutPresetComboBox->setCurrentIndex(mShortcutPresetComboBox->count() - 1);
        return;
    }

    if(settings->shortcutsModified()) {
        // editing away from a preset selects a dedicated "Custom" entry (see kCustomPresetEntryRole)
        mShortcutPresetComboBox->insertSeparator(mShortcutPresetComboBox->count());
        mShortcutPresetComboBox->addItem(tr("Custom"), current);
        mShortcutPresetComboBox->setItemData(mShortcutPresetComboBox->count() - 1, true, kCustomPresetEntryRole);
        mShortcutPresetComboBox->setCurrentIndex(mShortcutPresetComboBox->count() - 1);
    } else {
        mShortcutPresetComboBox->setCurrentIndex(currentPresetIndex);
    }
}
//------------------------------------------------------------------------------
// force the minimum width to fit contents
void SettingsDialog::adjustSizeToContents() {
    // general tab
    ui->gridLayout->activate();
    ui->horizontalLayout_28->activate();
    ui->horizontalLayout_19->activate();
    ui->gridLayout_3->activate();
    ui->horizontalLayout_18->activate();
    ui->gridLayout_4->activate();
    ui->horizontalLayout_24->activate();
    ui->gridLayout_5->activate();
    ui->slideshowGroupContents->activate();
    ui->scrollAreaWidgetContents->layout()->activate();
    ui->scrollArea->setMinimumWidth(ui->scrollAreaWidgetContents->minimumSizeHint().width());
    // view tab
    ui->horizontalLayout_29->activate();
    ui->horizontalLayout_31->activate();
    ui->widget->layout()->activate();
    ui->scrollAreaWidgetContents_3->layout()->activate();
    ui->scrollArea_3->setMinimumWidth(ui->scrollAreaWidgetContents_3->minimumSizeHint().width());
    // container
    this->setMinimumWidth(sizeHint().width() + 22);
}
//------------------------------------------------------------------------------
void SettingsDialog::setupSidebar() {

}
//------------------------------------------------------------------------------
void SettingsDialog::readSettings() {
    ui->loopSlideshowCheckBox->setChecked(settings->loopSlideshow());
    ui->videoPlaybackCheckBox->setChecked(settings->videoPlayback());
    ui->videoPlaybackGroupContents->setEnabled(settings->videoPlayback());
    ui->playSoundsCheckBox->setChecked(settings->playVideoSounds());
    ui->showControlsCheckBox->setChecked(settings->showVideoControls());
    ui->enablePanelCheckBox->setChecked(settings->panelEnabled());
    ui->thumbnailPanelGroupContents->setEnabled(settings->panelEnabled());
    ui->panelFullscreenOnlyCheckBox->setChecked(settings->panelFullscreenOnly());
    ui->squareThumbnailsCheckBox->setChecked(settings->squareThumbnails());
    ui->transparencyGridCheckBox->setChecked(settings->transparencyGrid());
    ui->enableSmoothScrollCheckBox->setChecked(settings->enableSmoothScroll());
    ui->usePreloaderCheckBox->setChecked(settings->usePreloader());
    ui->useThumbnailCacheCheckBox->setChecked(settings->useThumbnailCache());
    ui->smoothUpscalingCheckBox->setChecked(settings->smoothUpscaling());
    ui->expandImageCheckBox->setChecked(settings->expandImage());
    ui->expandImagesGroupContents->setEnabled(settings->expandImage());
    ui->smoothAnimatedImagesCheckBox->setChecked(settings->smoothAnimatedImages());
    ui->bgOpacitySlider->setValue(static_cast<int>(settings->backgroundOpacity() * 100));
    ui->blurBackgroundCheckBox->setChecked(settings->blurBackground());
    ui->sortingComboBox->setCurrentIndex(settings->sortingMode());
    ui->confirmDeleteCheckBox->setChecked(settings->confirmDelete());
    ui->confirmTrashCheckBox->setChecked(settings->confirmTrash());
    ui->unlockMinZoomCheckBox->setChecked(settings->unlockMinZoom());
    ui->sortFoldersCheckBox->setChecked(settings->sortFolders());
    ui->allowBrowseRootCheckBox->setChecked(settings->allowBrowseRoot());
    ui->trackpadDetectionCheckBox->setChecked(settings->trackpadDetection());
    ui->clickableEdgesCheckBox->setChecked(settings->clickableEdges());
    ui->clickableEdgesVisibleCheckBox->setChecked(settings->clickableEdgesVisible());
    ui->clickableEdgesVisibleCheckBox->setEnabled(settings->clickableEdges());
    ui->showHiddenFilesCheckBox->setChecked(settings->showHiddenFiles());
    ui->showOtherFileTypesCheckBox->setChecked(settings->showOtherFileTypes());

    if(settings->zoomIndicatorMode() == INDICATOR_ENABLED)
        ui->zoomIndicatorOn->setChecked(true);
    else if(settings->zoomIndicatorMode() == INDICATOR_AUTO)
        ui->zoomIndicatorAuto->setChecked(true);
    else
        ui->zoomIndicatorOff->setChecked(true);
    ui->showInfoBarFullscreen->setChecked(settings->infoBarFullscreen());
    ui->showInfoBarWindowed->setChecked(settings->infoBarWindowed());
    ui->showExtendedInfoTitle->setChecked(settings->windowTitleExtendedInfo());
    ui->showFullMetadataCheckBox->setChecked(settings->showFullMetadata());
    ui->cursorAutohideComboBox->setCurrentIndex(settings->cursorAutohide());
    ui->keepFitModeCheckBox->setChecked(settings->keepFitMode());
    if(settings->focusPointIn1to1Mode() == FOCUS_TOP)
        ui->focus1to1Top->setChecked(true);
    else if(settings->focusPointIn1to1Mode() == FOCUS_CENTER)
        ui->focus1to1Center->setChecked(true);
    else
        ui->focus1to1Cursor->setChecked(true);
    ui->slideshowIntervalSpinBox->setValue(settings->slideshowInterval());
    ui->imageScrollingComboBox->setCurrentIndex(settings->imageScrolling());
    ui->saveOverlayCheckBox->setChecked(settings->showSaveOverlay());
    ui->unloadThumbsCheckBox->setChecked(settings->unloadThumbs());
    if(settings->thumbPanelStyle() == TH_PANEL_SIMPLE)
        ui->thumbStyleSimple->setChecked(true);
    else
        ui->thumbStyleExtended->setChecked(true);
    ui->animatedJxlCheckBox->setChecked(settings->jxlAnimation());
    ui->autoResizeWindowCheckBox->setChecked(settings->autoResizeWindow());
    ui->panelCenterSelectionCheckBox->setChecked(settings->panelCenterSelection());
    ui->useFixedZoomLevelsCheckBox->setChecked(settings->useFixedZoomLevels());
    ui->zoomLevels->setText(settings->zoomLevels());

    if(settings->defaultViewMode() == MODE_FOLDERVIEW)
        ui->startInFolderViewCheckBox->setChecked(true);
    else
        ui->startInFolderViewCheckBox->setChecked(false);
    ui->folderViewTopBarCheckBox->setChecked(settings->folderViewTopBar());
    ui->folderViewFontSizeSpinBox->setValue(settings->folderViewFontPointSize());

    if(settings->folderEndAction() == FOLDER_END_NO_ACTION)
        ui->folderEndNoAction->setChecked(true);
    else if(settings->folderEndAction() == FOLDER_END_LOOP)
        ui->folderEndLoop->setChecked(true);
    else
        ui->folderEndSwitchFolder->setChecked(true);

    ui->mpvLineEdit->setText(settings->mpvBinary());

    ui->zoomStepSlider->setValue(static_cast<int>(settings->zoomStep() * 100.f));
    onZoomStepSliderChanged(ui->zoomStepSlider->value());

    ui->mouseScrollingSpeedSlider->setValue(static_cast<int>((settings->mouseScrollingSpeed() - 0.5f) / 0.25f));
    onMouseScrollingSpeedSliderChanged(ui->mouseScrollingSpeedSlider->value());

    ui->autoResizeLimitSlider->setValue(static_cast<int>(settings->autoResizeLimit() / 5.f));
    onAutoResizeLimitSliderChanged(ui->autoResizeLimitSlider->value());

    ui->JPEGQualitySlider->setValue(settings->JPEGSaveQuality());
    onJPEGQualitySliderChanged(ui->JPEGQualitySlider->value());

    ui->expandLimitSlider->setValue(settings->expandLimit());
    onExpandLimitSliderChanged(ui->expandLimitSlider->value());

    // thumbnailer and duplicate finder share a core budget: each max is cores minus the other's share
    int cores = qMax(1, QThread::idealThreadCount());
    ui->thumbnailerThreadsSlider->setMaximum(qMax(1, cores - settings->duplicateSearchThreadCount()));
    ui->duplicateThreadsSlider->setMaximum(qMax(1, cores - settings->thumbnailerThreadCount()));
    ui->thumbnailerThreadsSlider->setValue(settings->thumbnailerThreadCount());
    onThumbnailerThreadsSliderChanged(ui->thumbnailerThreadsSlider->value());
    ui->duplicateThreadsSlider->setValue(settings->duplicateSearchThreadCount());
    onDuplicateThreadsSliderChanged(ui->duplicateThreadsSlider->value());

    ui->memoryLimitSpinBox->setValue(settings->memoryAllocationLimit());

    ui->thumbMemCacheSpinBox->setValue(settings->thumbnailerMemCacheLimit());

    // language
    QString langName = langs.value(settings->language());
    // match on the display name: the combo is not editable
    if(langName.isEmpty() || ui->langComboBox->findText(langName) == -1)
        ui->langComboBox->setCurrentText(langs.value("en_US"));
    else
        ui->langComboBox->setCurrentText(langName);

    // ##### fit mode #####
    if(settings->imageFitMode() == FIT_WINDOW)
        ui->fitModeWindow->setChecked(true);
    else if(settings->imageFitMode() == FIT_WIDTH)
        ui->fitModeWidth->setChecked(true);
    else if(settings->imageFitMode() == FIT_WINDOW_STRETCH)
        ui->fitModeWindowStretch->setChecked(true);
    else
        ui->fitMode1to1->setChecked(true);

    // ##### UI #####
    mScalingFilterChoice = settings->scalingFilter();
    mScalingEngineComboBox->setCurrentIndex(settings->legacyScalingEngine() ? 1 : 0);
    mScalingSharpenCheckBox->setChecked(settings->scalingSharpen());
    updateScalingEngineControls();
    ui->fullscreenCheckBox->setChecked(settings->fullscreenMode());
    ui->pinPanelCheckBox->setChecked(settings->panelPinned());
    ui->panelPositionComboBox->setCurrentIndex(settings->panelPosition());

    // reduce by 10x to have nice granular control in qslider
    ui->panelSizeSlider->setValue(settings->panelPreviewsSize() / 10);

    readColorScheme();
    updateThemeControls(ui->themeSelectorComboBox->currentIndex());
    readShortcuts();
    readScripts();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettings() {
    // wait for all background stuff to finish
    if(QThreadPool::globalInstance()->activeThreadCount()) {
        QThreadPool::globalInstance()->waitForDone();
    }

    settings->setLoopSlideshow(ui->loopSlideshowCheckBox->isChecked());
    settings->setFullscreenMode(ui->fullscreenCheckBox->isChecked());
    if(ui->fitModeWindow->isChecked())
        settings->setImageFitMode(FIT_WINDOW);
    else if(ui->fitModeWidth->isChecked())
        settings->setImageFitMode(FIT_WIDTH);
    else if(ui->fitModeWindowStretch->isChecked())
        settings->setImageFitMode(FIT_WINDOW_STRETCH);
    else
        settings->setImageFitMode(FIT_ORIGINAL);

    settings->setLanguage(langs.key(ui->langComboBox->currentText()));

    settings->setVideoPlayback(ui->videoPlaybackCheckBox->isChecked());
    settings->setPlayVideoSounds(ui->playSoundsCheckBox->isChecked());
    settings->setShowVideoControls(ui->showControlsCheckBox->isChecked());
    settings->setPanelEnabled(ui->enablePanelCheckBox->isChecked());
    settings->setPanelFullscreenOnly(ui->panelFullscreenOnlyCheckBox->isChecked());
    settings->setSquareThumbnails(ui->squareThumbnailsCheckBox->isChecked());
    settings->setTransparencyGrid(ui->transparencyGridCheckBox->isChecked());
    settings->setShowHiddenFiles(ui->showHiddenFilesCheckBox->isChecked());
    settings->setShowOtherFileTypes(ui->showOtherFileTypesCheckBox->isChecked());
    settings->setEnableSmoothScroll(ui->enableSmoothScrollCheckBox->isChecked());
    settings->setUsePreloader(ui->usePreloaderCheckBox->isChecked());
    settings->setUseThumbnailCache(ui->useThumbnailCacheCheckBox->isChecked());
    settings->setSmoothUpscaling(ui->smoothUpscalingCheckBox->isChecked());
    settings->setExpandImage(ui->expandImageCheckBox->isChecked());
    settings->setSmoothAnimatedImages(ui->smoothAnimatedImagesCheckBox->isChecked());

    settings->setBackgroundOpacity(static_cast<qreal>(ui->bgOpacitySlider->value()) / 100.f);
    settings->setBlurBackground(ui->blurBackgroundCheckBox->isChecked());
    settings->setSortingMode(static_cast<SortingMode>(ui->sortingComboBox->currentIndex()));
    settings->setConfirmDelete(ui->confirmDeleteCheckBox->isChecked());
    settings->setConfirmTrash(ui->confirmTrashCheckBox->isChecked());
    settings->setUnlockMinZoom(ui->unlockMinZoomCheckBox->isChecked());
    settings->setSortFolders(ui->sortFoldersCheckBox->isChecked());
    settings->setAllowBrowseRoot(ui->allowBrowseRootCheckBox->isChecked());
    settings->setTrackpadDetection(ui->trackpadDetectionCheckBox->isChecked());
    settings->setClickableEdges(ui->clickableEdgesCheckBox->isChecked());
    settings->setClickableEdgesVisible(ui->clickableEdgesVisibleCheckBox->isChecked());

    if(ui->zoomIndicatorOn->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_ENABLED);
    else if(ui->zoomIndicatorAuto->isChecked())
        settings->setZoomIndicatorMode(INDICATOR_AUTO);
    else
        settings->setZoomIndicatorMode(INDICATOR_DISABLED);
    settings->setInfoBarFullscreen(ui->showInfoBarFullscreen->isChecked());
    settings->setInfoBarWindowed(ui->showInfoBarWindowed->isChecked());
    settings->setWindowTitleExtendedInfo(ui->showExtendedInfoTitle->isChecked());
    settings->setShowFullMetadata(ui->showFullMetadataCheckBox->isChecked());
    settings->setCursorAutohide(static_cast<CursorAutohideMode>(ui->cursorAutohideComboBox->currentIndex()));
    settings->setKeepFitMode(ui->keepFitModeCheckBox->isChecked());
    if(ui->focus1to1Top->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_TOP);
    else if(ui->focus1to1Center->isChecked())
        settings->setFocusPointIn1to1Mode(FOCUS_CENTER);
    else
        settings->setFocusPointIn1to1Mode(FOCUS_CURSOR);

    settings->setSlideshowInterval(ui->slideshowIntervalSpinBox->value());

    if(ui->startInFolderViewCheckBox->isChecked())
        settings->setDefaultViewMode(MODE_FOLDERVIEW);
    else
        settings->setDefaultViewMode(MODE_DOCUMENT);
    settings->setFolderViewTopBar(ui->folderViewTopBarCheckBox->isChecked());
    settings->setFolderViewFontPointSize(ui->folderViewFontSizeSpinBox->value());

    if(ui->folderEndNoAction->isChecked())
        settings->setFolderEndAction(FOLDER_END_NO_ACTION);
    else if(ui->folderEndLoop->isChecked())
        settings->setFolderEndAction(FOLDER_END_LOOP);
    else
        settings->setFolderEndAction(FOLDER_END_GOTO_ADJACENT);

    settings->setMpvBinary(ui->mpvLineEdit->text());
    settings->setScalingFilter(mScalingFilterChoice);
    settings->setLegacyScalingEngine(mScalingEngineComboBox->currentIndex() == 1);
    settings->setScalingSharpen(mScalingSharpenCheckBox->isChecked());
    settings->setImageScrolling(static_cast<ImageScrolling>(ui->imageScrollingComboBox->currentIndex()));
    settings->setShowSaveOverlay(ui->saveOverlayCheckBox->isChecked());
    settings->setUnloadThumbs(ui->unloadThumbsCheckBox->isChecked());
    if(ui->thumbStyleSimple->isChecked())
        settings->setThumbPanelStyle(TH_PANEL_SIMPLE);
    else
        settings->setThumbPanelStyle(TH_PANEL_EXTENDED);
    settings->setJxlAnimation(ui->animatedJxlCheckBox->isChecked());
    settings->setAutoResizeWindow(ui->autoResizeWindowCheckBox->isChecked());
    settings->setPanelCenterSelection(ui->panelCenterSelectionCheckBox->isChecked());
    settings->setUseFixedZoomLevels(ui->useFixedZoomLevelsCheckBox->isChecked());
    settings->setZoomLevels(ui->zoomLevels->text());

    settings->setPanelPinned(ui->pinPanelCheckBox->isChecked());
    int panelPos = ui->panelPositionComboBox->currentIndex();
    settings->setPanelPosition(static_cast<PanelPosition>(panelPos));

    settings->setPanelPreviewsSize(ui->panelSizeSlider->value() * 10);

    settings->setJPEGSaveQuality(ui->JPEGQualitySlider->value());
    settings->setZoomStep(static_cast<qreal>(ui->zoomStepSlider->value() / 100.f));
    settings->setMouseScrollingSpeed(static_cast<qreal>(0.5f + (ui->mouseScrollingSpeedSlider->value() * 0.25f)));
    settings->setAutoResizeLimit(ui->autoResizeLimitSlider->value() * 5);
    settings->setExpandLimit(ui->expandLimitSlider->value());
    // setters clamp against each other, so write the finder count on both sides of the thumbnailer write
    settings->setDuplicateSearchThreadCount(ui->duplicateThreadsSlider->value());
    settings->setThumbnailerThreadCount(ui->thumbnailerThreadsSlider->value());
    settings->setDuplicateSearchThreadCount(ui->duplicateThreadsSlider->value());
    settings->setMemoryAllocationLimit(ui->memoryLimitSpinBox->value());
    settings->setThumbnailerMemCacheLimit(ui->thumbMemCacheSpinBox->value());

    // System derives from the desktop and is never written to the theme file
    if(ui->themeSelectorComboBox->currentIndex() == 6) {
        settings->setColorScheme(ThemeStore::colorScheme(COLORS_SYSTEM));
        settings->saveTheme();
    }
    else {
        saveColorScheme();
    }
    // the saved palette becomes the new revert baseline for later previews
    mSchemeAtOpen = settings->colorScheme();
    mThemePreviewed = false;
    saveShortcuts();
    refreshShortcutPresetCombo(); // reflect Custom/clean immediately, not just on reopen

    scriptManager->saveScripts();
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveSettingsAndClose() {
    saveSettings();
    this->close();
}
//------------------------------------------------------------------------------
void SettingsDialog::readColorScheme() {
    auto colors = settings->colorScheme();
    // seed from the active palette if custom, else the stored custom palette
    mCustomColors = (colors.tid == COLORS_CUSTOM) ? colors : settings->customColorScheme();
    mCustomColors.tid = COLORS_CUSTOM;
    setColorScheme(colors);
    mPrevThemeIndex = ui->themeSelectorComboBox->currentIndex();
}

void SettingsDialog::setColorScheme(ColorScheme colors) {
    QSignalBlocker blocker(ui->themeSelectorComboBox);
    switch (colors.tid) {
        case COLORS_LIGHT:     ui->themeSelectorComboBox->setCurrentIndex(3); break;
        case COLORS_BLACK:     ui->themeSelectorComboBox->setCurrentIndex(0); break;
        case COLORS_DARK:      ui->themeSelectorComboBox->setCurrentIndex(1); break;
        case COLORS_DARKBLUE:  ui->themeSelectorComboBox->setCurrentIndex(2); break;
        case COLORS_LIGHT_YELLOW:  ui->themeSelectorComboBox->setCurrentIndex(4); break;
        case COLORS_CUSTOM:    ui->themeSelectorComboBox->setCurrentIndex(5); break;
        case COLORS_SYSTEM:    ui->themeSelectorComboBox->setCurrentIndex(6); break;
        default:               ui->themeSelectorComboBox->setCurrentIndex(-1); break;
    }
    ui->colorSelectorAccent->setColor(colors.accent);
    ui->colorSelectorBackground->setColor(colors.background);
    ui->colorSelectorFullscreen->setColor(colors.background_fullscreen);
    ui->colorSelectorFolderview->setColor(colors.folderview);
    ui->colorSelectorFolderviewPanel->setColor(colors.folderview_topbar);
    ui->colorSelectorText->setColor(colors.text);
    ui->colorSelectorIcons->setColor(colors.icons);
    ui->colorSelectorWidget->setColor(colors.widget);
    ui->colorSelectorWidgetBorder->setColor(colors.widget_border);
    ui->colorSelectorOverlay->setColor(colors.overlay);
    ui->colorSelectorOverlayText->setColor(colors.overlay_text);
    ui->colorSelectorScrollbar->setColor(colors.scrollbar);
    ui->folderViewCellBackgroundColorSelector->setColor(colors.folderview_cell_bg);
    ui->folderViewLabelBackgroundColorSelector->setColor(colors.folderview_label_bg);
    ui->folderViewSelectedLabelBackgroundColorSelector->setColor(colors.folderview_selected_label_bg);
    ui->folderViewSelectionColorSelector->setColor(colors.folderview_selection);
    ui->folderViewParentIconColorSelector->setColor(colors.folderview_parent_icon);
}

//------------------------------------------------------------------------------
ColorScheme SettingsDialog::collectColorScheme() {
    BaseColorScheme base;
    base.accent = ui->colorSelectorAccent->color();
    base.background = ui->colorSelectorBackground->color();
    base.background_fullscreen = ui->colorSelectorFullscreen->color();
    base.folderview = ui->colorSelectorFolderview->color();
    base.folderview_topbar = ui->colorSelectorFolderviewPanel->color();
    base.text = ui->colorSelectorText->color();
    base.icons = ui->colorSelectorIcons->color();
    base.widget = ui->colorSelectorWidget->color();
    base.widget_border = ui->colorSelectorWidgetBorder->color();
    base.overlay = ui->colorSelectorOverlay->color();
    base.overlay_text = ui->colorSelectorOverlayText->color();
    base.scrollbar = ui->colorSelectorScrollbar->color();
    base.folderview_cell_bg = ui->folderViewCellBackgroundColorSelector->color();
    base.folderview_label_bg = ui->folderViewLabelBackgroundColorSelector->color();
    base.folderview_selected_label_bg = ui->folderViewSelectedLabelBackgroundColorSelector->color();
    base.folderview_selection = ui->folderViewSelectionColorSelector->color();
    base.folderview_parent_icon = ui->folderViewParentIconColorSelector->color();
    base.tid = selectedThemeTid();
    return ColorScheme(base);
}
//------------------------------------------------------------------------------
void SettingsDialog::saveColorScheme() {
    settings->setColorScheme(collectColorScheme());
    settings->saveTheme();
}
//------------------------------------------------------------------------------
// live re-theme without touching disk; exec() reverts it unless saved
void SettingsDialog::applyColorSchemePreview() {
    settings->setColorScheme(collectColorScheme());
    mThemePreviewed = true;
    emit settingsChanged();
}
//------------------------------------------------------------------------------
void SettingsDialog::markThemeCustom() {
    QSignalBlocker blocker(ui->themeSelectorComboBox);
    ui->themeSelectorComboBox->setCurrentIndex(5);
    mPrevThemeIndex = 5;
}
//------------------------------------------------------------------------------
int SettingsDialog::selectedThemeTid() const {
    switch(ui->themeSelectorComboBox->currentIndex()) {
        case 0: return COLORS_BLACK;
        case 1: return COLORS_DARK;
        case 2: return COLORS_DARKBLUE;
        case 3: return COLORS_LIGHT;
        case 4: return COLORS_LIGHT_YELLOW;
        case 5: return COLORS_CUSTOM;
        case 6: return COLORS_SYSTEM;
        default: return COLORS_CUSTOM;
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::updateThemeControls(int index) {
    const bool system = (index == 6);  // "System" is the last dropdown entry
    ui->colorConfigSubgroup->setEnabled(!system);
    ui->modifySystemSchemeLabel->setVisible(system);
}
//------------------------------------------------------------------------------
void SettingsDialog::readShortcuts() {
    mShortcutDraft = actionManager->allShortcuts();
    rebuildShortcutDraftLookup();
    settings->readShortcutPrimary(mShortcutPrimary);
    settings->readDisabledShortcuts(mShortcutDisabled);
    mShortcutFallback = actionManager->scrollFallbackOverrides();
    mShortcutVideoFallback = actionManager->videoScrollFallbackOverrides();
    updateShortcutsTable();
    refreshShortcutPresetCombo();
}
//------------------------------------------------------------------------------
namespace {
// User-facing context names, matching the labels in the context combo boxes.
QString contextLabel(ViewMode context) {
    if(context == MODE_GLOBAL)
        return QObject::tr("Global");
    if(context == MODE_COPYMOVE)
        return QObject::tr("Copy / Move");
    return (context == MODE_FOLDERVIEW) ? QObject::tr("Grid") : QObject::tr("Document");
}
// Script actions are stored as "s:<name>"; never show the raw id to the user.
QString shortcutActionLabel(const QString &action) {
    return action.startsWith(QStringLiteral("s:"))
               ? QObject::tr("%1  (script)").arg(action.mid(2))
               : action;
}
}    // namespace
//------------------------------------------------------------------------------
QString SettingsDialog::shortcutActionAtRow(int row) const {
    // read the action id from the context cell, the one column always present
    QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, ShortcutColumnContext);
    return item ? item->data(Qt::UserRole).toString() : QString();
}
//------------------------------------------------------------------------------
// Space toggles the row's Enabled box whichever cell is current
bool SettingsDialog::eventFilter(QObject *watched, QEvent *event) {
    if(watched == ui->shortcutsTableWidget && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if((keyEvent->key() == Qt::Key_Space || keyEvent->key() == Qt::Key_Select) &&
           keyEvent->modifiers() == Qt::NoModifier) {
            QTableWidgetItem *item = ui->shortcutsTableWidget->item(
                ui->shortcutsTableWidget->currentRow(), ShortcutColumnEnabled);
            if(item && (item->flags() & Qt::ItemIsUserCheckable)) {
                item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
                return true;
            }
        }
    }
    return QDialog::eventFilter(watched, event);
}
//------------------------------------------------------------------------------
void SettingsDialog::setCurrentShortcutRow(ViewMode context, const QString &action, int column) {
    for(int row = 0; row < ui->shortcutsTableWidget->rowCount(); row++) {
        if(shortcutActionAtRow(row) == action && shortcutContextAtRow(row) == context) {
            ui->shortcutsTableWidget->setCurrentCell(row, column);
            return;
        }
    }
}
//------------------------------------------------------------------------------
ViewMode SettingsDialog::shortcutContextAtRow(int row) const {
    QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, ShortcutColumnContext);
    if(!item)
        return MODE_GLOBAL;
    return static_cast<ViewMode>(item->data(kShortcutContextRole).toInt());
}
//------------------------------------------------------------------------------
// monochrome art recolored to the theme; Global uses the globe
QIcon SettingsDialog::shortcutContextIcon(ViewMode context) const {
    auto cached = mShortcutContextIcons.constFind(context);
    if(cached != mShortcutContextIcons.constEnd())
        return cached.value();

    QString name;
    if(context == MODE_FOLDERVIEW)
        name = QStringLiteral("folderview16");
    else if(context == MODE_DOCUMENT)
        name = QStringLiteral("document-view16");
    else if(context == MODE_COPYMOVE)
        name = QStringLiteral("copy16");
    else
        name = QStringLiteral("global16");

    const QColor color = settings->colorScheme().icons;
    QIcon icon;
    // both scales so HiDPI gets the @2x art
    for(const QString &suffix : {QStringLiteral(""), QStringLiteral("@2x")}) {
        QPixmap pixmap(QStringLiteral(":/res/icons/common/menuitem/%1%2.png").arg(name, suffix));
        if(pixmap.isNull())
            continue;
        ImageLib::recolor(pixmap, color);
        icon.addPixmap(pixmap);
    }
    mShortcutContextIcons.insert(context, icon);
    return icon;
}
//------------------------------------------------------------------------------
QStringList SettingsDialog::actionShortcuts(const ActionManager::ContextMap &map, const QString &action) const {
    QStringList keys = map.keys(action);
    keys.sort(Qt::CaseInsensitive);
    return keys;
}
//------------------------------------------------------------------------------
void SettingsDialog::setActionShortcuts(ActionManager::ContextMap &map, const QString &action, const QStringList &keys) {
    for(auto it = map.begin(); it != map.end();) {
        if(it.value() == action)
            it = map.erase(it);
        else
            ++it;
    }

    for(const QString &key : keys) {
        const QString trimmed = key.trimmed();
        if(!trimmed.isEmpty())
            map.insert(trimmed, action);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::rebuildShortcutDraftLookup() {
    mShortcutDraftLookup.clear();
    for(auto ctx = mShortcutDraft.cbegin(); ctx != mShortcutDraft.cend(); ++ctx) {
        QHash<QString, QString> lookup;
        for(auto it = ctx.value().cbegin(); it != ctx.value().cend(); ++it)
            lookup.insert(it.key(), it.value());
        mShortcutDraftLookup.insert(ctx.key(), lookup);
    }
}
//------------------------------------------------------------------------------
QString SettingsDialog::draftActionForShortcut(ViewMode context, const QString &shortcut) const {
    const QString key = shortcut.trimmed();
    if(key.isEmpty())
        return QString();
    const QString contextAction = mShortcutDraftLookup.value(context).value(key);
    // Global bindings don't reach the Copy / Move overlay's own keys
    if(!contextAction.isEmpty() || context == MODE_GLOBAL || ActionManager::isOverlayContext(context))
        return contextAction;
    return mShortcutDraftLookup.value(MODE_GLOBAL).value(key);
}
//------------------------------------------------------------------------------
// the draft is the authoritative key set, so a removed default key stays removed
QStringList SettingsDialog::candidateShortcuts(ViewMode context, const QString &action) const {
    return actionShortcuts(mShortcutDraft.value(context), action);
}
//------------------------------------------------------------------------------
QStringList SettingsDialog::defaultShortcuts(ViewMode context, const QString &action) const {
    return actionShortcuts(actionManager->allDefaultShortcuts().value(context), action);
}
//------------------------------------------------------------------------------
QString SettingsDialog::primaryShortcut(ViewMode context, const QString &action) const {
    const QStringList candidates = candidateShortcuts(context, action);
    if(candidates.isEmpty())
        return QString();
    const QString savedPrimary = mShortcutPrimary.value(context).value(action);
    if(candidates.contains(savedPrimary))
        return savedPrimary;
    // no explicit choice: prefer a user key, else the first default, keyboard before mouse
    const QStringList defaults = defaultShortcuts(context, action);
    for(const QString &key : candidates)
        if(!defaults.contains(key))
            return key;
    const auto isMouseInput = [](const QString &key) {
        const QString input = key.section('+', -1);
        for(const char *mouse : {"LMB", "RMB", "MiddleButton", "XButton", "Wheel"})
            if(input.startsWith(QLatin1String(mouse)))
                return true;
        return false;
    };
    for(const QString &key : candidates)
        if(!isMouseInput(key))
            return key;
    return candidates.first();
}
//------------------------------------------------------------------------------
void SettingsDialog::setPrimaryShortcut(ViewMode context, const QString &action, const QString &key) {
    if(key.isEmpty())
        return;
    mShortcutPrimary[context].insert(action, key);
}
//------------------------------------------------------------------------------
bool SettingsDialog::shortcutEnabled(ViewMode context, const QString &action) const {
    return !mShortcutDisabled.value(context).contains(action);
}
//------------------------------------------------------------------------------
void SettingsDialog::setShortcutEnabled(ViewMode context, const QString &action, bool enabled) {
    QStringList disabled = mShortcutDisabled.value(context);
    if(enabled) {
        disabled.removeAll(action);
        mShortcutDisabled[context] = disabled;
        // disabling clears the keys, so re-enabling restores defaults (plus a remembered primary)
        QStringList keys = candidateShortcuts(context, action);
        if(keys.isEmpty()) {
            keys = defaultShortcuts(context, action);
            const QString primary = mShortcutPrimary.value(context).value(action);
            if(!primary.isEmpty() && !keys.contains(primary))
                keys.append(primary);
        }
        setActionShortcuts(mShortcutDraft[context], action, keys);
    } else {
        if(!disabled.contains(action))
            disabled.append(action);
        disabled.sort(Qt::CaseInsensitive);
        mShortcutDisabled[context] = disabled;
        setActionShortcuts(mShortcutDraft[context], action, QStringList());
    }
    rebuildShortcutDraftLookup();
}
//------------------------------------------------------------------------------
void SettingsDialog::updateShortcutsTable() {
    if(!mShortcutSearchEdit)
        return;

    mUpdatingShortcutsTable = true;
    ui->shortcutsTableWidget->setSortingEnabled(false);
    ui->shortcutsTableWidget->clearContents();
    ui->shortcutsTableWidget->setRowCount(0);

    const QBrush disabledBrush = palette().brush(QPalette::Disabled, QPalette::Text);
    // widest scope first
    const QList<ViewMode> contexts{MODE_GLOBAL, MODE_FOLDERVIEW, MODE_DOCUMENT, MODE_COPYMOVE};
    for(int order = 0; order < contexts.size(); order++) {
        const ViewMode context = contexts.at(order);
        const ActionManager::ContextMap defaults = actionManager->allDefaultShortcuts().value(context);
        const ActionManager::ContextMap active = mShortcutDraft.value(context);

        QSet<QString> actions;
        for(auto it = defaults.cbegin(); it != defaults.cend(); ++it)
            actions.insert(it.value());
        for(auto it = active.cbegin(); it != active.cend(); ++it)
            actions.insert(it.value());
        // list disabled actions too, or Enabled would be a one-way switch
        const QStringList disabledHere = mShortcutDisabled.value(context);
        for(const QString &action : disabledHere)
            actions.insert(action);
        // seed script rows (Global-only, usually unbound) so they are bindable here
        if(context == MODE_GLOBAL && scriptManager) {
            const QStringList names = scriptManager->scriptNames();
            for(const QString &name : names)
                actions.insert(QStringLiteral("s:") + name);
        }

        QStringList actionNames = actions.values();
        actionNames.sort(Qt::CaseInsensitive);

        for(const QString &action : actionNames) {
            const bool enabled = shortcutEnabled(context, action);
            const QString primary = primaryShortcut(context, action);
            const QString label = shortcutActionLabel(action);

            const int row = ui->shortcutsTableWidget->rowCount();
            ui->shortcutsTableWidget->setRowCount(row + 1);

            // icon only; the label is in the tooltip and searchable
            QTableWidgetItem *contextItem = new ShortcutContextItem(order, label);
            contextItem->setIcon(shortcutContextIcon(context));
            contextItem->setToolTip(contextLabel(context));
            contextItem->setData(kShortcutContextLabelRole, contextLabel(context));
            contextItem->setTextAlignment(Qt::AlignCenter);
            ui->shortcutsTableWidget->setItem(row, ShortcutColumnContext, contextItem);

            // script rows show a "(script)" label; Qt::UserRole keeps the real id
            const bool isScript = action.startsWith(QStringLiteral("s:"));

            QTableWidgetItem *actionItem = new QTableWidgetItem(label);
            if(isScript)
                actionItem->setToolTip(tr("User script \"%1\"").arg(action.mid(2)));
            ui->shortcutsTableWidget->setItem(row, ShortcutColumnAction, actionItem);

            QTableWidgetItem *keyItem = new QTableWidgetItem(primary);
            if(!enabled)
                keyItem->setForeground(disabledBrush);
            ui->shortcutsTableWidget->setItem(row, ShortcutColumnKey, keyItem);

            QTableWidgetItem *countItem = new QTableWidgetItem();
            // Store as int so the column sorts numerically rather than lexically.
            countItem->setData(Qt::DisplayRole, candidateShortcuts(context, action).size());
            countItem->setTextAlignment(Qt::AlignCenter);
            if(!enabled)
                countItem->setForeground(disabledBrush);
            ui->shortcutsTableWidget->setItem(row, ShortcutColumnCount, countItem);

            QTableWidgetItem *enabledItem = new QTableWidgetItem();
            enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable);
            enabledItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
            enabledItem->setTextAlignment(Qt::AlignCenter);
            ui->shortcutsTableWidget->setItem(row, ShortcutColumnEnabled, enabledItem);

            // sorting reorders rows, so every cell carries the row identity
            for(int column = 0; column < ShortcutColumnTotal; column++) {
                QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, column);
                item->setData(Qt::UserRole, action);
                item->setData(kShortcutContextRole, static_cast<int>(context));
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            }
        }
    }

    ui->shortcutsTableWidget->setSortingEnabled(true);
    // clamp a column index from an older layout
    const int sortColumn = qBound(0, settings->shortcutsSortColumn(), ShortcutColumnTotal - 1);
    ui->shortcutsTableWidget->sortByColumn(sortColumn, settings->shortcutsSortOrder());
    mUpdatingShortcutsTable = false;
    updateShortcutsFilter();
}
//------------------------------------------------------------------------------
void SettingsDialog::updateShortcutsFilter() {
    if(!mShortcutSearchEdit)
        return;

    const QString needle = mShortcutSearchEdit->text().trimmed();
    for(int row = 0; row < ui->shortcutsTableWidget->rowCount(); row++) {
        bool match = needle.isEmpty();
        for(int col = ShortcutColumnAction; col <= ShortcutColumnKey && !match; col++) {
            QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, col);
            match = item && item->text().contains(needle, Qt::CaseInsensitive);
        }
        // the context name lives only in item data; match it too
        if(!match) {
            QTableWidgetItem *item = ui->shortcutsTableWidget->item(row, ShortcutColumnContext);
            match = item && item->data(kShortcutContextLabelRole).toString().contains(needle, Qt::CaseInsensitive);
        }
        ui->shortcutsTableWidget->setRowHidden(row, !match);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::openShortcutDetails(int row) {
    const QString action = shortcutActionAtRow(row);
    if(!action.isEmpty())
        openShortcutDetails(action, shortcutContextAtRow(row));
}
//------------------------------------------------------------------------------
void SettingsDialog::openShortcutDetails(const QString &action, ViewMode context) {
    QDialog dialog(this);
    dialog.setObjectName("ShortcutDetailsDialog");
    dialog.setWindowTitle(shortcutActionLabel(action));
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);

    QLabel *hint = new QLabel(
        tr("Select the primary key. Add or remove keys below; system defaults are marked."),
        &dialog);
    hint->setWordWrap(true);
    mainLayout->addWidget(hint);

    // default keys drive the badge and primaryShortcut()'s default
    const QStringList defaultKeys = defaultShortcuts(context, action);
    const QSet<QString> defaultSet(defaultKeys.cbegin(), defaultKeys.cend());

    // Working copies edited in place; committed to the model only on accept.
    QStringList keys = candidateShortcuts(context, action);
    QString primary = primaryShortcut(context, action);

    // The key rows live in their own widget so add/remove can rebuild just them.
    QWidget *rowsWidget = new QWidget(&dialog);
    QVBoxLayout *rowsLayout = new QVBoxLayout(rowsWidget);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(rowsWidget);

    // deferred deletion: safe from inside a child button's click handler
    std::function<void(QLayout *)> clearLayout = [&](QLayout *layout) {
        QLayoutItem *item;
        while((item = layout->takeAt(0)) != nullptr) {
            if(item->widget()) {
                item->widget()->hide();
                item->widget()->deleteLater();
            } else if(item->layout()) {
                clearLayout(item->layout());
                item->layout()->deleteLater();
            }
            delete item;
        }
    };

    std::function<void()> rebuild = [&]() {
        clearLayout(rowsLayout);
        if(keys.isEmpty()) {
            rowsLayout->addWidget(new QLabel(tr("No keys assigned."), rowsWidget));
            return;
        }
        for(const QString &key : keys) {
            QHBoxLayout *row = new QHBoxLayout();
            QRadioButton *radio = new QRadioButton(key, rowsWidget);
            // exclusivity is enforced by rebuild(), so deferred-deleted radios can't interfere
            radio->setAutoExclusive(false);
            radio->setChecked(key == primary);
            connect(radio, &QRadioButton::clicked, &dialog, [&, key]() {
                primary = key;
                rebuild();
            });
            row->addWidget(radio);
            if(defaultSet.contains(key)) {
                QLabel *badge = new QLabel(tr("default"), rowsWidget);
                badge->setEnabled(false);
                row->addWidget(badge);
            }
            row->addStretch();
            QToolButton *del = new QToolButton(rowsWidget);
            del->setObjectName("shortcutKeyRemove");
            del->setText(tr("Remove"));
            connect(del, &QToolButton::clicked, &dialog, [&, key]() {
                keys.removeAll(key);
                if(primary == key)
                    primary = keys.isEmpty() ? QString() : keys.first();
                rebuild();
            });
            row->addWidget(del);
            rowsLayout->addLayout(row);
        }
    };
    rebuild();

    // Add-key row: pressing a key sequence captures it and makes it primary.
    QHBoxLayout *addRow = new QHBoxLayout();
    addRow->addWidget(new QLabel(tr("Add key:"), &dialog));
    KeySequenceEdit *adder = new KeySequenceEdit(&dialog);
    addRow->addWidget(adder, 1);
    addRow->addWidget(adder->createMouseGestureButton(&dialog));
    mainLayout->addLayout(addRow);

    QLabel *warning = new QLabel(&dialog);
    warning->setWordWrap(true);
    mainLayout->addWidget(warning);

    // what a scroll action does when there is nothing to scroll (and in a video)
    const auto addFallbackRow = [&](const QString &caption, const QString &objectName,
                                    const QString &defaultFallback, const QString &saved) {
        QHBoxLayout *fallbackRow = new QHBoxLayout();
        fallbackRow->addWidget(new QLabel(caption, &dialog));
        QComboBox *box = new QComboBox(&dialog);
        box->setObjectName(objectName);
        const auto label = [&](const QString &name, const QString &text) {
            return name == defaultFallback ? tr("%1 (default)").arg(text) : text;
        };
        box->addItem(label(QString(), tr("Nothing")), QString());
        QStringList candidates = appActions->getList();
        candidates.sort(Qt::CaseInsensitive);
        for(const QString &candidate : candidates) {
            if(!ActionManager::hasScrollFallback(candidate) && !candidate.startsWith(QLatin1String("scrollOr")))
                box->addItem(label(candidate, shortcutActionLabel(candidate)), candidate);
        }
        const int current = box->findData(saved == QLatin1String("none") ? QString() : saved);
        box->setCurrentIndex(current < 0 ? 0 : current);
        fallbackRow->addWidget(box, 1);
        mainLayout->addLayout(fallbackRow);
        return box;
    };
    QComboBox *fallbackBox = nullptr;
    const QString defaultFallback = ActionManager::defaultScrollFallback(action);
    if(ActionManager::hasScrollFallback(action)) {
        fallbackBox = addFallbackRow(tr("When there is nothing to scroll:"), "shortcutFallbackComboBox",
                                     defaultFallback, mShortcutFallback.value(action, defaultFallback));
    }
    QComboBox *videoFallbackBox = nullptr;
    const QString defaultVideoFallback = ActionManager::defaultVideoScrollFallback(action);
    if(ActionManager::hasVideoScrollFallback(action)) {
        videoFallbackBox = addFallbackRow(tr("In a video:"), "shortcutVideoFallbackComboBox", defaultVideoFallback,
                                          mShortcutVideoFallback.value(action, defaultVideoFallback));
    }

    connect(adder, &KeySequenceEdit::edited, &dialog, [&]() {
        const QString seq = adder->sequence();
        if(seq.isEmpty())
            return;
        if(keys.contains(seq)) {
            warning->setText(tr("\"%1\" is already assigned to this action.").arg(seq));
            return;
        }
        const QString clash = draftActionForShortcut(context, seq);
        warning->setText(clash.isEmpty() || clash == action
                             ? QString()
                             : tr("\"%1\" is also used by: %2").arg(seq, clash));
        keys.append(seq);
        keys.sort(Qt::CaseInsensitive);
        primary = seq;    // newly added key becomes primary unless changed
        rebuild();
    });

    // QDialogButtonBox order is style-driven; wire OK/Cancel ourselves (accept first)
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addStretch();
    QPushButton *okButton = new QPushButton(tr("OK"), &dialog);
    QPushButton *cancelButton = new QPushButton(tr("Cancel"), &dialog);
    okButton->setDefault(true);
    for(QPushButton *b : {okButton, cancelButton}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setIcon(QIcon());
    }
    connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonsLayout->addWidget(okButton);
    buttonsLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonsLayout);

    if(dialog.exec() != QDialog::Accepted)
        return;

    if(fallbackBox) {
        const QString chosen = fallbackBox->currentData().toString();
        if(chosen == defaultFallback)
            mShortcutFallback.remove(action);
        else
            mShortcutFallback.insert(action, chosen.isEmpty() ? QStringLiteral("none") : chosen);
    }
    if(videoFallbackBox) {
        const QString chosen = videoFallbackBox->currentData().toString();
        if(chosen == defaultVideoFallback)
            mShortcutVideoFallback.remove(action);
        else
            mShortcutVideoFallback.insert(action, chosen.isEmpty() ? QStringLiteral("none") : chosen);
    }
    // Commit the edited key set and primary choice into the draft model.
    setActionShortcuts(mShortcutDraft[context], action, keys);
    rebuildShortcutDraftLookup();
    if(primary.isEmpty())
        mShortcutPrimary[context].remove(action);
    else
        setPrimaryShortcut(context, action, primary);
    // Assigning keys implies the action should be active again.
    if(!keys.isEmpty()) {
        QStringList disabled = mShortcutDisabled.value(context);
        if(disabled.removeAll(action) > 0)
            mShortcutDisabled[context] = disabled;
    }
    updateShortcutsTable();
}
//------------------------------------------------------------------------------
QMap<QString, QString> SettingsDialog::shortcutTransferClashes(const QString &action,
                                                               ViewMode src, ViewMode dst) const {
    QMap<QString, QString> clashes;
    if(src == dst)
        return clashes;
    const QStringList keys = candidateShortcuts(src, action);
    for(const QString &key : keys) {
        // also catches the destination shadowing a global key
        const QString taken = draftActionForShortcut(dst, key);
        if(!taken.isEmpty() && taken != action)
            clashes.insert(key, taken);
    }
    return clashes;
}
//------------------------------------------------------------------------------
void SettingsDialog::applyShortcutTransfer(const QString &action, ViewMode src, ViewMode dst, bool move) {
    if(action.isEmpty() || src == dst)
        return;
    const QStringList keys = candidateShortcuts(src, action);
    if(keys.isEmpty())
        return;    // nothing bound here to relocate
    const QString primary = primaryShortcut(src, action);

    // union: keep the destination's own keys; keys taken from another action were confirmed
    QStringList target = candidateShortcuts(dst, action);
    for(const QString &key : keys)
        if(!target.contains(key))
            target.append(key);
    target.sort(Qt::CaseInsensitive);
    setActionShortcuts(mShortcutDraft[dst], action, target);

    // the primary key travels with the binding
    if(!primary.isEmpty())
        setPrimaryShortcut(dst, action, primary);

    // a transfer implies enabled at the destination
    QStringList dstDisabled = mShortcutDisabled.value(dst);
    if(dstDisabled.removeAll(action) > 0)
        mShortcutDisabled[dst] = dstDisabled;

    if(move) {
        setActionShortcuts(mShortcutDraft[src], action, QStringList());
        // move primary too: re-enabling re-adds a remembered primary and would resurrect the key
        mShortcutPrimary[src].remove(action);
        QStringList srcDisabled = mShortcutDisabled.value(src);
        if(srcDisabled.removeAll(action) > 0)
            mShortcutDisabled[src] = srcDisabled;
    }

    rebuildShortcutDraftLookup();
    updateShortcutsTable();
}
//------------------------------------------------------------------------------
void SettingsDialog::transferShortcut(const QString &action, ViewMode src, ViewMode dst, bool move) {
    if(action.isEmpty() || src == dst || candidateShortcuts(src, action).isEmpty())
        return;

    QStringList lines;
    lines << (move ? tr("Move \"%1\" from %2 to %3.") : tr("Copy \"%1\" from %2 to %3."))
                 .arg(shortcutActionLabel(action), contextLabel(src), contextLabel(dst));

    // crossing the Global boundary changes where the shortcut reaches; say so
    if(dst == MODE_GLOBAL)
        lines << tr("This widens its reach: it will run in every screen, not only in the %1 view "
                    "(a view-specific binding for the same key still overrides it).")
                     .arg(contextLabel(src));
    else if(src == MODE_GLOBAL && move)
        lines << tr("This narrows its reach: it runs in every screen today, and afterwards "
                    "it will run only in the %1 view.").arg(contextLabel(dst));
    else if(src == MODE_GLOBAL)
        lines << tr("The global binding stays as it is; the %1 copy simply overrides it "
                    "while that view is active.").arg(contextLabel(dst));
    else
        lines << (move ? tr("It will run only in the %1 view instead of the %2 view.")
                             .arg(contextLabel(dst), contextLabel(src))
                       : tr("It will run in the %1 view as well as the %2 view.")
                             .arg(contextLabel(dst), contextLabel(src)));

    const QMap<QString, QString> clashes = shortcutTransferClashes(action, src, dst);
    for(auto it = clashes.cbegin(); it != clashes.cend(); ++it)
        lines << tr("\"%1\" is also used by: %2").arg(it.key(), shortcutActionLabel(it.value()));
    if(!clashes.isEmpty())
        lines << tr("Continuing takes those keys over in the %1 context.").arg(contextLabel(dst));

    lines << tr("Continue?");
    if(!CustomMessageBox::confirm(this, move ? tr("Move shortcut") : tr("Copy shortcut"),
                                  lines.join(QStringLiteral("\n\n"))))
        return;
    applyShortcutTransfer(action, src, dst, move);
}
//------------------------------------------------------------------------------
// already in viewport coordinates; remapping would shift by the header height
QString SettingsDialog::shortcutActionAtMenuPos(const QPoint &viewportPos) const {
    const int row = ui->shortcutsTableWidget->rowAt(viewportPos.y());
    // Cells carry the real action id in UserRole (script rows show a label).
    return (row < 0) ? QString() : shortcutActionAtRow(row);
}
//------------------------------------------------------------------------------
ViewMode SettingsDialog::shortcutContextAtMenuPos(const QPoint &viewportPos) const {
    const int row = ui->shortcutsTableWidget->rowAt(viewportPos.y());
    return (row < 0) ? MODE_GLOBAL : shortcutContextAtRow(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::showShortcutRowMenu(const QPoint &pos) {
    const QString action = shortcutActionAtMenuPos(pos);
    if(action.isEmpty())
        return;
    const ViewMode src = shortcutContextAtMenuPos(pos);

    QMenu menu(this);
    menu.addAction(tr("Edit keys..."), this, [this, action, src]() {
        openShortcutDetails(action, src);
    });
    menu.addSeparator();

    QMenu *moveMenu = menu.addMenu(tr("Move to"));
    QMenu *copyMenu = menu.addMenu(tr("Copy to"));
    if(candidateShortcuts(src, action).isEmpty() || ActionManager::isOverlayContext(src)) {
        // disabled with a reason rather than absent; Copy / Move keys only work in that overlay
        const QString reason = (src == MODE_COPYMOVE) ? tr("Only used in the Copy / Move overlay")
                                                      : tr("No keys bound in %1").arg(contextLabel(src));
        moveMenu->addAction(reason)->setEnabled(false);
        copyMenu->addAction(reason)->setEnabled(false);
    } else {
        // scripts are allowed in any context
        const QList<ViewMode> contexts{MODE_GLOBAL, MODE_FOLDERVIEW, MODE_DOCUMENT};
        for(ViewMode dst : contexts) {
            if(dst == src)
                continue;
            moveMenu->addAction(contextLabel(dst), this, [this, action, src, dst]() {
                transferShortcut(action, src, dst, true);
            });
            copyMenu->addAction(contextLabel(dst), this, [this, action, src, dst]() {
                transferShortcut(action, src, dst, false);
            });
        }
    }
    menu.exec(ui->shortcutsTableWidget->viewport()->mapToGlobal(pos));
}
//------------------------------------------------------------------------------
void SettingsDialog::readScripts() {
    ui->scriptsListWidget->clear();
    const QMap<QString, Script> scripts = scriptManager->allScripts();
    QMapIterator<QString, Script> i(scripts);
    while(i.hasNext()) {
        i.next();
        addScriptToList(i.key());
    }
}
//------------------------------------------------------------------------------
// does not check for an existing entry
void SettingsDialog::addScriptToList(const QString &name) {
    if(name.isEmpty())
        return;

    QListWidget *list = ui->scriptsListWidget;
    // empty item text avoids ghosted text under the row widget; the name is in Qt::UserRole
    QListWidgetItem *nameItem = new QListWidgetItem();
    nameItem->setData(Qt::UserRole, name);
    nameItem->setSizeHint(QSize(0, 26));
    list->insertItem(list->count(), nameItem);

    // opaque container so the icon buttons get their clicks; it re-emits clicks for selection and edit
    ScriptRowWidget *row = new ScriptRowWidget();
    row->setObjectName(QStringLiteral("scriptRow"));
    connect(row, &ScriptRowWidget::clicked, this,
            [this, nameItem]() { ui->scriptsListWidget->setCurrentItem(nameItem); });
    connect(row, &ScriptRowWidget::doubleClicked, this,
            [this, name]() { editScript(name); });
    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(8, 0, 4, 0);
    rowLayout->setSpacing(2);

    QLabel *nameLabel = new QLabel(name, row);
    nameLabel->setObjectName(QStringLiteral("scriptRowLabel"));
    // Let clicks on the label reach the row container (for selection / edit).
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(nameLabel);
    rowLayout->addStretch(1);

    IconButton *editBtn = new IconButton(row);
    editBtn->setObjectName(QStringLiteral("scriptRowEdit"));
    editBtn->setIconPath(QStringLiteral(":res/icons/common/overlay/edit16.png"));
    editBtn->setFixedSize(22, 22);
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setToolTip(tr("Edit"));
    connect(editBtn, &IconButton::clicked, this, [this, name]() { editScript(name); });
    rowLayout->addWidget(editBtn);

    IconButton *deleteBtn = new IconButton(row);
    deleteBtn->setObjectName(QStringLiteral("scriptRowDelete"));
    deleteBtn->setIconPath(QStringLiteral(":res/icons/common/overlay/close16.png"));
    deleteBtn->setFixedSize(22, 22);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setToolTip(tr("Delete"));
    connect(deleteBtn, &IconButton::clicked, this, [this, name]() { removeScript(name); });
    rowLayout->addWidget(deleteBtn);

    list->setItemWidget(nameItem, row);
}
//------------------------------------------------------------------------------
void SettingsDialog::addScript() {
    ScriptEditorDialog w;
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript() {
    QListWidgetItem *item = ui->scriptsListWidget->currentItem();
    if(item)
        editScript(item);
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(QListWidgetItem* item) {
    if(item) {
        editScript(item->data(Qt::UserRole).toString());
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editScript(const QString& name) {
    ScriptEditorDialog w(name, scriptManager->getScript(name));
    if(w.exec()) {
        if(w.scriptName().isEmpty())
            return;
        scriptManager->addScript(w.scriptName(), w.script());
        readScripts();
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::removeScript(const QString& name) {
    if(name.isEmpty() || !scriptManager->scriptExists(name))
        return;
    // purge from every per-context structure, or the deleted script returns as a ghost row
    const QString action = QStringLiteral("s:") + name;
    const QList<ViewMode> contexts{MODE_GLOBAL, MODE_FOLDERVIEW, MODE_DOCUMENT};
    for(ViewMode context : contexts) {
        setActionShortcuts(mShortcutDraft[context], action, QStringList());
        mShortcutPrimary[context].remove(action);
        QStringList disabled = mShortcutDisabled.value(context);
        if(disabled.removeAll(action) > 0)
            mShortcutDisabled[context] = disabled;
    }
    rebuildShortcutDraftLookup();
    saveShortcuts();
    readShortcuts();
    scriptManager->removeScript(name);
    readScripts();
}
//------------------------------------------------------------------------------
void SettingsDialog::addShortcut() {
    ShortcutCreatorDialog w;
    if(!w.exec())
        return;

    const ViewMode context = w.selectedContext();
    const QString key = w.selectedShortcut().trimmed();
    const QString action = w.selectedAction();
    if(key.isEmpty() || action.isEmpty())
        return;

    mShortcutDraft[context].remove(key);
    mShortcutDraft[context].insert(key, action);
    rebuildShortcutDraftLookup();
    setPrimaryShortcut(context, action, key);
    setShortcutEnabled(context, action, true);
    // a leftover search could hide the new row
    if(mShortcutSearchEdit)
        mShortcutSearchEdit->clear();
    updateShortcutsTable();

    // scroll to the new row
    for(int row = 0; row < ui->shortcutsTableWidget->rowCount(); row++) {
        if(shortcutActionAtRow(row) != action || shortcutContextAtRow(row) != context)
            continue;
        ui->shortcutsTableWidget->setCurrentCell(row, ShortcutColumnAction);
        ui->shortcutsTableWidget->scrollToItem(ui->shortcutsTableWidget->item(row, ShortcutColumnAction),
                                               QAbstractItemView::PositionAtCenter);
        break;
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut(int row) {
    openShortcutDetails(row);
}
//------------------------------------------------------------------------------
void SettingsDialog::editShortcut() {
    editShortcut(ui->shortcutsTableWidget->currentRow());
}
//------------------------------------------------------------------------------
void SettingsDialog::removeShortcut() {
    const int row = ui->shortcutsTableWidget->currentRow();
    const QString action = (row < 0) ? QString() : shortcutActionAtRow(row);
    if(action.isEmpty())
        return;
    const ViewMode context = shortcutContextAtRow(row);
    setActionShortcuts(mShortcutDraft[context], action, QStringList());
    rebuildShortcutDraftLookup();
    mShortcutPrimary[context].remove(action);
    updateShortcutsTable();
}
//------------------------------------------------------------------------------
void SettingsDialog::saveShortcuts() {
    actionManager->removeAllShortcuts();
    for(auto ctx = mShortcutDraft.cbegin(); ctx != mShortcutDraft.cend(); ++ctx) {
        for(auto it = ctx.value().cbegin(); it != ctx.value().cend(); ++it)
            actionManager->addShortcut(ctx.key(), it.key(), it.value());
    }
    actionManager->saveShortcuts();
    settings->saveShortcutPrimary(mShortcutPrimary);
    actionManager->setScrollFallbackOverrides(mShortcutFallback);
    actionManager->setVideoScrollFallbackOverrides(mShortcutVideoFallback);
    settings->saveDisabledShortcuts(mShortcutDisabled);
}
//------------------------------------------------------------------------------
void SettingsDialog::resetZoomLevels() {
    ui->zoomLevels->setText(settings->defaultZoomLevels());
}
//------------------------------------------------------------------------------
void SettingsDialog::selectMpvPath() {
    QFileDialog dialog;
    QString file;
    file = dialog.getOpenFileName(this, tr("Navigate to mpv binary"), "", "mpv*");
    if(!file.isEmpty()) {
        ui->mpvLineEdit->setText(file);
    }
}
//------------------------------------------------------------------------------
void SettingsDialog::onExpandLimitSliderChanged(int value) {
    if(value == 0)
        ui->expandLimitLabel->setText("-");
    else
        ui->expandLimitLabel->setText(QString::number(value) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onJPEGQualitySliderChanged(int value) {
    ui->JPEGQualityLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onZoomStepSliderChanged(int value) {
    ui->zoomStepLabel->setText(QString::number(value / 100.f, 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onMouseScrollingSpeedSliderChanged(int value) {
    ui->mouseScrollingSpeedLabel->setText(QString::number(0.5f + (value*0.25f), 'f', 2) + "x");
}
//------------------------------------------------------------------------------
void SettingsDialog::onThumbnailerThreadsSliderChanged(int value) {
    ui->thumbnailerThreadsLabel->setText(QString::number(value));
    // live-rebalance: the finder slider may only use what the thumbnailer leaves
    int cores = qMax(1, QThread::idealThreadCount());
    ui->duplicateThreadsSlider->setMaximum(qMax(1, cores - value));
}

void SettingsDialog::onDuplicateThreadsSliderChanged(int value) {
    ui->duplicateThreadsLabel->setText(QString::number(value));
    int cores = qMax(1, QThread::idealThreadCount());
    ui->thumbnailerThreadsSlider->setMaximum(qMax(1, cores - value));
}
//------------------------------------------------------------------------------
void SettingsDialog::onBgOpacitySliderChanged(int value) {
    ui->bgOpacityPercentLabel->setText(QString::number(value) + "%");
}
//------------------------------------------------------------------------------
void SettingsDialog::onAutoResizeLimitSliderChanged(int value) {
    ui->autoResizeLimit->setText(QString::number(value * 5.f, 'f', 0) + "%");
}
//------------------------------------------------------------------------------
int SettingsDialog::exec() {
    mSchemeAtOpen = settings->colorScheme();
    mThemePreviewed = false;
    int res = QDialog::exec();
    // OK and Cancel both go through close(); saveSettings() clears the flag when previews are kept
    if(mThemePreviewed) {
        mThemePreviewed = false;
        settings->setColorScheme(mSchemeAtOpen);
        emit settingsChanged();
    }
    return res;
}

void SettingsDialog::switchToPage(int number) {
    ui->sideBar2->selectEntry(number);
}

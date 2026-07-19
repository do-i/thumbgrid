#pragma once

#include <QDialog>
#include <QFileDialog>
#include <QColorDialog>
#include <QThreadPool>
#include <QTableWidget>
#include <QTextBrowser>
#include <QListWidget>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QApplication>
#include <QDebug>
#include <QMenu>
#include <QComboBox>
#include <QHash>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include "gui/customwidgets/colorselectorbutton.h"
#include "gui/dialogs/shortcutcreatordialog.h"
#include "gui/dialogs/scripteditordialog.h"
#include "settings.h"
#include "components/actionmanager/actionmanager.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;
    void switchToPage(int number);

    // --- Shortcuts page: the draft binding model -----------------------------
    // Everything here reads and writes the in-dialog draft only; nothing reaches
    // ActionManager until Apply/OK commits it through saveShortcuts(). Public
    // because the table's row context menu drives the transfer commands and the
    // behavior tests exercise them without going through a modal menu.
    QStringList candidateShortcuts(ViewMode context, const QString &action) const;
    QString primaryShortcut(ViewMode context, const QString &action) const;
    void setPrimaryShortcut(ViewMode context, const QString &action, const QString &key);
    bool shortcutEnabled(ViewMode context, const QString &action) const;
    void setShortcutEnabled(ViewMode context, const QString &action, bool enabled);
    // Keys that `action` holds in `src` which `dst` has already given to some
    // *other* action, as key -> conflicting action. Empty means a clean transfer.
    // Non-global destinations also report keys taken by a Global binding, since
    // the new view-specific binding would shadow it.
    QMap<QString, QString> shortcutTransferClashes(const QString &action, ViewMode src, ViewMode dst) const;
    // Draft-map surgery behind Move to.../Copy to...: carries the action's key
    // set, its primary-key choice and its enabled state across contexts, and for
    // a move clears all three in the source. Does not prompt - transferShortcut()
    // is the confirm-gated wrapper the menu uses.
    void applyShortcutTransfer(const QString &action, ViewMode src, ViewMode dst, bool move);
    // Resolves the action id of the shortcuts-table row at a position from the
    // table's customContextMenuRequested signal - viewport coordinates, used
    // as-is (see the definition). Empty when no row is there.
    QString shortcutActionAtMenuPos(const QPoint &viewportPos) const;

public slots:
    int exec() override;

private:
    void readColorScheme();
    void setColorScheme(ColorScheme colors);
    ColorScheme collectColorScheme();
    void saveColorScheme();
    void applyColorSchemePreview();
    void markThemeCustom();
    int selectedThemeTid() const;
    // Enable/disable the colour editor and toggle the "modify" hint depending on
    // whether the "System" preset is selected in the theme dropdown.
    void updateThemeControls(int index);
    // user's custom palette, preserved while a preset is previewed
    ColorScheme mCustomColors;
    // active palette to restore if the dialog closes with unsaved live previews
    ColorScheme mSchemeAtOpen;
    bool mThemePreviewed = false;
    int mPrevThemeIndex = -1;
    void readSettings();
    void readShortcuts();
    void readScripts();
    Ui::SettingsDialog *ui;

    void saveShortcuts();
    void addScriptToList(const QString &name);
    void setupPreferencePages();
    QWidget* makeSettingsPage(const QString &title, QVBoxLayout **contentLayout);
    QWidget* makeSettingsGroup(const QString &title = QString());
    void setupShortcutsPage();
    void setupAboutPage();
    void setupThemePage();
    void setupFeatureToggles();
    void setupMiscControls();
    // Repopulates the preset dropdown from ActionManager::availablePresets(),
    // selects the active preset, and labels it "(modified)" if the active
    // mapping has diverged from it.
    void refreshShortcutPresetCombo();
    void updateShortcutsTable();
    void updateShortcutsFilter();
    void setActionShortcuts(ActionManager::ContextMap &map, const QString &action, const QStringList &keys);
    QStringList actionShortcuts(const ActionManager::ContextMap &map, const QString &action) const;
    QStringList defaultShortcuts(ViewMode context, const QString &action) const;
    void rebuildShortcutDraftLookup();
    QString draftActionForShortcut(ViewMode context, const QString &shortcut) const;
    ViewMode selectedShortcutContext() const;
    void openShortcutDetails(int row);
    void openShortcutDetails(const QString &action, ViewMode context);
    // Right-click on a shortcuts-table row: edit keys, plus Move to.../Copy to...
    void showShortcutRowMenu(const QPoint &pos);
    // Explains the scope change (and any key it would take over), then applies.
    void transferShortcut(const QString &action, ViewMode src, ViewMode dst, bool move);

    void setupSidebar();
    void adjustSizeToContents();
    QMap<QString, QString> langs; // <"en_US", "English">
    QButtonGroup fitModeGrp, folderEndGrp, zoomIndGrp;
    ActionManager::ShortcutMap mShortcutDraft;
    QMap<ViewMode, QHash<QString, QString>> mShortcutDraftLookup;
    QMap<ViewMode, QMap<QString, QString>> mShortcutPrimary;
    QMap<ViewMode, QStringList> mShortcutDisabled;
    QComboBox *mShortcutContextComboBox = nullptr;
    QLineEdit *mShortcutSearchEdit = nullptr;
    QComboBox *mShortcutPresetComboBox = nullptr;
    bool mUpdatingShortcutsTable = false;

private slots:
    void saveSettings();
    void saveSettingsAndClose();

    void addScript();
    void editScript();
    void editScript(QListWidgetItem *item);
    void editScript(const QString& name);
    void removeScript(const QString& name);

    void addShortcut();
    void editShortcut();
    void editShortcut(int row);
    void removeShortcut();
    void selectMpvPath();
    void onBgOpacitySliderChanged(int value);
    void onThumbnailerThreadsSliderChanged(int value);
    void onDuplicateThreadsSliderChanged(int value);
    void onExpandLimitSliderChanged(int value);
    void onZoomStepSliderChanged(int value);
    void onJPEGQualitySliderChanged(int value);
    void resetToDesktopTheme();    
    void onAutoResizeLimitSliderChanged(int value);
    void onMouseScrollingSpeedSliderChanged(int value);

    void resetZoomLevels();
signals:
    void settingsChanged();
};

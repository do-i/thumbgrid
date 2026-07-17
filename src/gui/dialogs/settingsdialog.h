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
    void addShortcutToTable(const QString &action, const QString &shortcut, ViewMode context);
    void addScriptToList(const QString &name);
    // The shortcut table carries a Context column (stable token in Qt::UserRole).
    ViewMode contextAtRow(int row);
    void selectShortcutRow(const QString &shortcut, ViewMode context);
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
    QStringList candidateShortcuts(ViewMode context, const QString &action) const;
    QStringList defaultShortcuts(ViewMode context, const QString &action) const;
    QString primaryShortcut(ViewMode context, const QString &action) const;
    void setPrimaryShortcut(ViewMode context, const QString &action, const QString &key);
    bool shortcutEnabled(ViewMode context, const QString &action) const;
    void setShortcutEnabled(ViewMode context, const QString &action, bool enabled);
    void rebuildShortcutDraftLookup();
    QString draftActionForShortcut(ViewMode context, const QString &shortcut) const;
    ViewMode selectedShortcutContext() const;
    void openShortcutDetails(int row);
    void openShortcutDetails(const QString &action, ViewMode context);

    void setupSidebar();
    void removeShortcutAt(int row);
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
    void editScript(QString name);
    void removeScript();

    void addShortcut();
    void editShortcut();
    void editShortcut(int row);
    void removeShortcut();
    void resetShortcuts();
    void selectMpvPath();
    void onBgOpacitySliderChanged(int value);
    void onThumbnailerThreadsSliderChanged(int value);
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

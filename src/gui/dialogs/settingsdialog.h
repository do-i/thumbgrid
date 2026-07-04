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
    ~SettingsDialog();
    void switchToPage(int number);

public slots:
    int exec();

private:
    void readColorScheme();
    void setColorScheme(ColorScheme colors);
    ColorScheme collectColorScheme();
    void saveColorScheme();
    void applyColorSchemePreview();
    void markThemeCustom();
    int selectedThemeTid() const;
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

    void setupSidebar();
    void removeShortcutAt(int row);
    void adjustSizeToContents();
    QMap<QString, QString> langs; // <"en_US", "English">
    QButtonGroup fitModeGrp, folderEndGrp, zoomIndGrp;

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

#pragma once

#include <QDebug>
#include <QColor>
#include <QPalette>

class QSettings;

enum ColorSchemes {
    COLORS_SYSTEM,
    COLORS_LIGHT,
    COLORS_BLACK,
    COLORS_DARK,
    COLORS_DARKBLUE,
    COLORS_CUSTOM,
    // 6 is reserved: an interim build stored COLORS_CUSTOM there and
    // normalizedThemeTid() still folds 5/6 into COLORS_CUSTOM.
    COLORS_LIGHT_YELLOW = 7
};

struct BaseColorScheme {
    int tid;
    QColor background;
    QColor background_fullscreen;
    QColor text;
    QColor icons;
    QColor widget;
    QColor widget_border;
    QColor accent;
    QColor folderview;
    QColor folderview_topbar;
    QColor folderview_label_bg;
    QColor folderview_selection;
    QColor folderview_parent_icon;
    QColor folderview_selected_label_bg;
    QColor folderview_cell_bg;
    QColor scrollbar;
    QColor overlay_text;
    QColor overlay;
};

class ColorScheme {
public:
    ColorScheme();
    ColorScheme(BaseColorScheme base);
    void setBaseColors(BaseColorScheme base);
    // index of theme name
    int tid;
    // base
    QColor background;
    QColor background_fullscreen;
    QColor text;
    QColor icons;
    QColor widget;
    QColor widget_border;
    QColor accent;
    QColor folderview;
    QColor folderview_topbar;
    QColor folderview_label_bg;
    QColor folderview_selection;
    QColor folderview_parent_icon;
    QColor folderview_selected_label_bg;
    QColor folderview_cell_bg;
    QColor scrollbar;
    QColor scrollbar_hover;
    QColor overlay_text;
    QColor overlay;
    // extended
    QColor text_hc2;
    QColor text_hc;
    QColor text_lc;
    QColor text_lc2;
    QColor button;
    QColor button_hover;
    QColor button_pressed;
    QColor panel_button;
    QColor panel_button_hover;
    QColor panel_button_pressed;
    QColor folderview_hc;
    QColor folderview_hc2;
    QColor folderview_button_hover;
    QColor folderview_button_pressed;
    QColor input_field_focus;


private:
    void createColorVariants();
};

class ThemeStore {
public:
    static ColorScheme colorScheme(ColorSchemes name);
    // Reads a [Colors] group into a BaseColorScheme. Keys that are absent are
    // left invalid so ColorScheme::setBaseColors() computes their variant.
    static BaseColorScheme parseColors(QSettings &settings, int tid);

private:
    static BaseColorScheme loadTheme(const QString &path, int tid);
};

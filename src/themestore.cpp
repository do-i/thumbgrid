#include "themestore.h"

#include <QSettings>
#include <QFile>
#include <QTemporaryFile>

namespace {

// Fixed preset -> file mapping. COLORS_SYSTEM / COLORS_CUSTOM are derived from
// the palette / user config and are not file-backed.
QString themeFileName(ColorSchemes name) {
    switch(name) {
        case COLORS_LIGHT:        return QStringLiteral("light.ini");
        case COLORS_BLACK:        return QStringLiteral("black.ini");
        case COLORS_DARK:         return QStringLiteral("dark.ini");
        case COLORS_DARKBLUE:     return QStringLiteral("darkblue.ini");
        case COLORS_LIGHT_YELLOW: return QStringLiteral("light_blue.ini");
        default:                  return QString();
    }
}

// Prefer the installed system config copy (admin-editable), fall back to the
// copy embedded in the binary via resources.qrc so dev/non-Linux builds work.
QString resolveThemePath(const QString &fileName) {
#ifdef THEMES_PATH
    const QString systemPath = QStringLiteral(THEMES_PATH) + "/" + fileName;
    if(QFile::exists(systemPath))
        return systemPath;
#endif
    return QStringLiteral(":/res/themes/") + fileName;
}

// Offset a color's HSV value by delta (clamped), preserving hue/saturation.
// Used to derive table surface colors from the widget background when a preset
// omits the explicit table_* keys.
QColor valueOffset(const QColor &c, int delta) {
    QColor r;
    r.setHsv(c.hue(), c.saturation(), qBound(0, c.value() + delta, 255));
    return r;
}

} // namespace

BaseColorScheme ThemeStore::parseColors(QSettings &settings, int tid) {
    BaseColorScheme base = {-1};
    base.tid = tid;
    settings.beginGroup("Colors");
    auto read = [&settings](const char *key, QColor &field) {
        if(settings.contains(key)) {
            QColor color(settings.value(key).toString());
            if(color.isValid())
                field = color;
        }
    };
    read("background",            base.background);
    read("background_fullscreen", base.background_fullscreen);
    read("text",                  base.text);
    read("icons",                 base.icons);
    read("widget",                base.widget);
    read("widget_border",         base.widget_border);
    read("accent",                base.accent);
    read("folderview",            base.folderview);
    read("folderview_topbar",     base.folderview_topbar);
    read("scrollbar",             base.scrollbar);
    read("overlay_text",          base.overlay_text);
    read("overlay",               base.overlay);
    read("fv_label_bg",           base.folderview_label_bg);
    read("fv_selection",          base.folderview_selection);
    read("fv_parent_icon",        base.folderview_parent_icon);
    read("fv_sel_label_bg",       base.folderview_selected_label_bg);
    read("fv_cell_bg",            base.folderview_cell_bg);
    read("table_bg",              base.table_bg);
    read("table_bg_alt",          base.table_bg_alt);
    read("table_header",          base.table_header);
    read("table_text",            base.table_text);
    read("table_border",          base.table_border);
    settings.endGroup();
    return base;
}

BaseColorScheme ThemeStore::loadTheme(const QString &path, int tid) {
    // QSettings cannot reliably open a read-only qrc resource directly, so copy
    // the embedded bytes to a temp file before parsing. Real filesystem paths
    // (the installed /etc copy) are read in place.
    if(path.startsWith(':')) {
        QFile resource(path);
        if(resource.open(QIODevice::ReadOnly)) {
            QTemporaryFile tmp;
            if(tmp.open()) {
                tmp.write(resource.readAll());
                tmp.flush();
                QSettings settings(tmp.fileName(), QSettings::IniFormat);
                return parseColors(settings, tid);
            }
        }
        qWarning() << "ThemeStore: failed to read embedded theme" << path;
        return {tid};
    }
    QSettings settings(path, QSettings::IniFormat);
    return parseColors(settings, tid);
}

ColorScheme ThemeStore::colorScheme(ColorSchemes name) {
    if(name == COLORS_SYSTEM || name == COLORS_CUSTOM) {
        BaseColorScheme base = {-1};
        QPalette p;
        base.background = p.window().color();
        base.background_fullscreen = p.window().color();
        base.folderview_topbar = p.window().color();
        base.widget = p.window().color();
        base.widget_border = p.window().color();
        base.folderview = p.base().color();
        base.text = p.text().color();
        base.icons = p.text().color();
        base.accent = p.highlight().color();
        base.overlay = p.window().color();
        base.overlay_text = p.text().color();
        base.scrollbar.setHsv(p.highlight().color().hue(),
                              qBound(0, p.highlight().color().saturation() - 20, 240),
                              qBound(0, p.highlight().color().value() - 35, 240));
        base.tid = static_cast<int>(name);
        return ColorScheme(base);
    }

    const QString fileName = themeFileName(name);
    if(fileName.isEmpty())
        return ColorScheme(BaseColorScheme{static_cast<int>(name)});
    return ColorScheme(loadTheme(resolveThemePath(fileName), static_cast<int>(name)));
}

//---------------------------------------------------------------------

ColorScheme::ColorScheme() {
    tid = -1;
}

ColorScheme::ColorScheme(BaseColorScheme base) {
    setBaseColors(base);
}

void ColorScheme::setBaseColors(BaseColorScheme base) {
    background            = base.background;
    background_fullscreen = base.background_fullscreen;
    text                  = base.text;
    icons                 = base.icons;
    widget                = base.widget;
    widget_border         = base.widget_border;
    accent                = base.accent;
    folderview            = base.folderview;
    folderview_topbar     = base.folderview_topbar;
    overlay               = base.overlay;
    overlay_text          = base.overlay_text;
    scrollbar             = base.scrollbar;
    tid = base.tid;
    createColorVariants();
    folderview_label_bg          = base.folderview_label_bg.isValid() ? base.folderview_label_bg : folderview_hc;
    folderview_selection         = base.folderview_selection.isValid() ? base.folderview_selection : accent;
    folderview_parent_icon       = base.folderview_parent_icon.isValid() ? base.folderview_parent_icon : icons;
    folderview_selected_label_bg = base.folderview_selected_label_bg.isValid() ? base.folderview_selected_label_bg : folderview_label_bg;
    folderview_cell_bg           = base.folderview_cell_bg.isValid() ? base.folderview_cell_bg : folderview;

    // Settings dialog shortcuts table. Presets/custom configs that omit these
    // keys must still render a themed table (not native white), so derive the
    // surface from the widget background: dark themes get a slightly-lighter
    // surface with a lighter header/gridline; light themes keep the native-ish
    // white look with a light-gray header and grid.
    if(widget.valueF() <= 0.45f) { // dark
        table_bg     = base.table_bg.isValid()     ? base.table_bg     : valueOffset(widget, 8);
        table_bg_alt = base.table_bg_alt.isValid() ? base.table_bg_alt : valueOffset(table_bg, 6);
        table_header = base.table_header.isValid() ? base.table_header : valueOffset(widget, 18);
        table_border = base.table_border.isValid() ? base.table_border : valueOffset(widget, 30);
    } else { // light
        table_bg     = base.table_bg.isValid()     ? base.table_bg     : widget;
        table_bg_alt = base.table_bg_alt.isValid() ? base.table_bg_alt : valueOffset(widget, -6);
        table_header = base.table_header.isValid() ? base.table_header : valueOffset(widget, -10);
        table_border = base.table_border.isValid() ? base.table_border : valueOffset(widget, -22);
    }
    table_text = base.table_text.isValid() ? base.table_text : text;
}

QString ColorScheme::bakedThumbnailSignature() const {
    // folderview_parent_icon recolors the folder icon; widget/widget_border/
    // text/text_lc2 are used by the rendered file-type icons. Keep this in sync
    // with ThumbnailerRunnable::renderFileTypeIcon / dirIconBase.
    return folderview_parent_icon.name(QColor::HexArgb) +
           widget.name(QColor::HexArgb) +
           widget_border.name(QColor::HexArgb) +
           text.name(QColor::HexArgb) +
           text_lc2.name(QColor::HexArgb);
}

void ColorScheme::createColorVariants() {
    if(widget.valueF() <= 0.45f) { // dark theme
        // top bar buttons
        panel_button.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMin(folderview_topbar.value() + 20, 255));
        panel_button_hover.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMin(folderview_topbar.value() + 26, 255));
        panel_button_pressed.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMin(folderview_topbar.value() + 15, 255));
        folderview_hc.setHsv(folderview.hue(), folderview.saturation(), qMin(folderview.value() + 12, 255));
        folderview_hc2.setHsv(folderview.hue(), folderview.saturation(), qMin(folderview.value() + 28, 255));
        folderview_button_pressed = folderview_hc;
        folderview_button_hover = folderview_hc2;
        // regular buttons - from widget bg
        button.setHsv(widget.hue(), widget.saturation(), qMin(widget.value() + 21, 255));
        button_hover    = QColor(button.lighter(112));
        button_pressed  = QColor(button.darker(112));
        scrollbar_hover = scrollbar.lighter(120);
        // text
        text_hc = QColor(text.lighter(110));
        text_hc2 = QColor(text.lighter(118));
        text_lc = QColor(text.darker(115));
        text_lc2 = QColor(text.darker(160));
    } else { // light theme
        // top bar buttons
        panel_button.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMax(folderview_topbar.value() - 30, 0));
        panel_button_hover.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMax(folderview_topbar.value() - 45, 0));
        panel_button_pressed.setHsv(folderview_topbar.hue(), folderview_topbar.saturation(), qMax(folderview_topbar.value() - 55, 0));
        folderview_hc.setHsv(folderview.hue(), folderview.saturation(), qMax(folderview.value() - 25, 0));
        folderview_hc2.setHsv(folderview.hue(), folderview.saturation(), qMax(folderview.value() - 60, 0));
        folderview_button_pressed = folderview_hc2;
        folderview_button_hover = folderview_hc;
        // regular buttons - from widget bg
        button.setHsv(widget.hue(), widget.saturation(), qMax(widget.value() - 42, 0));
        button_hover    = QColor(button.darker(106));
        button_pressed  = QColor(button.darker(118));
        scrollbar_hover = scrollbar.darker(120);
        // text
        text_hc = QColor(text.darker(104));
        text_hc2 = QColor(text.darker(112));
        text_lc = QColor(text.lighter(130));
        text_lc2 = QColor(text.lighter(160));
    }
    // misc
    input_field_focus = QColor(accent);
}

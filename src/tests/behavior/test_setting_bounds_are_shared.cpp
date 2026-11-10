#include "support/thumbgrid_test_support.h"

#include "constants.h"
#include "gui/dialogs/duplicatefinderdialog.h"
#include "gui/dialogs/settingsdialog.h"

#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>

// Sliders, settings getters and stored values agree on one set of bounds from
// constants.h. Stored values inside the bounds survive upgrade and a dialog
// Apply unchanged; values outside are clamped by the getter.

class SettingBoundsAreSharedTest : public QObject {
    Q_OBJECT

private slots:
    void slidersUseTheBounds();
    void gettersClampOnlyOutOfRangeValues();
    void applyKeepsInRangeValues();
};

using namespace tg::bounds;

namespace {

void store(const QString &key, const QVariant &value) {
    QSettings conf;
    conf.setValue(key, value);
}

} // namespace

void SettingBoundsAreSharedTest::slidersUseTheBounds() {
    SettingsDialog dialog;
    auto zoom = dialog.findChild<QSlider *>("zoomStepSlider");
    auto panel = dialog.findChild<QSlider *>("panelSizeSlider");
    QVERIFY(zoom && panel);
    QCOMPARE(zoom->minimum(), kZoomStepMinPct);
    QCOMPARE(zoom->maximum(), kZoomStepMaxPct);
    QCOMPARE(panel->minimum() * kPanelThumbSizeSliderScale, kPanelThumbSizeMinPx);
    QCOMPARE(panel->maximum() * kPanelThumbSizeSliderScale, kPanelThumbSizeMaxPx);

    DuplicateFinderDialog duplicates;
    auto spin = duplicates.findChild<QSpinBox *>();
    auto slider = duplicates.findChild<QSlider *>();
    QVERIFY(spin && slider);
    QCOMPARE(spin->minimum(), kDuplicateSimilarityMinPct);
    QCOMPARE(spin->maximum(), kDuplicateSimilarityMaxPct);
    QCOMPARE(slider->minimum(), kDuplicateSimilarityMinPct);
    QCOMPARE(slider->maximum(), kDuplicateSimilarityMaxPct);
}

void SettingBoundsAreSharedTest::gettersClampOnlyOutOfRangeValues() {
    store("Document/panelPreviewsSize", 170);
    store("Grid/folderViewIconSize", 130);
    store("Document/zoomStep", 0.29f);
    store("State/duplicateFinderSimilarity", 78);
    QCOMPARE(settings->panelPreviewsSize(), 170);
    QCOMPARE(settings->folderViewIconSize(), 130);
    QCOMPARE(settings->zoomStep(), 0.29f);
    QCOMPARE(settings->duplicateFinderSimilarity(), 78);

    store("Document/panelPreviewsSize", 20);
    store("Grid/folderViewIconSize", 10);
    store("Document/zoomStep", 0.001f);
    store("State/duplicateFinderSimilarity", 50);
    QCOMPARE(settings->panelPreviewsSize(), kPanelThumbSizeMinPx);
    QCOMPARE(settings->folderViewIconSize(), kGridThumbSizeMinPx);
    QCOMPARE(settings->zoomStep(), kZoomStepMin);
    QCOMPARE(settings->duplicateFinderSimilarity(), kDuplicateSimilarityMinPct);

    store("Document/panelPreviewsSize", 999);
    store("Grid/folderViewIconSize", 999);
    store("Document/zoomStep", 3.0f);
    store("State/duplicateFinderSimilarity", 120);
    QCOMPARE(settings->panelPreviewsSize(), kPanelThumbSizeMaxPx);
    QCOMPARE(settings->folderViewIconSize(), kGridThumbSizeMaxPx);
    QCOMPARE(settings->zoomStep(), kZoomStepMax);
    QCOMPARE(settings->duplicateFinderSimilarity(), kDuplicateSimilarityMaxPct);
}

void SettingBoundsAreSharedTest::applyKeepsInRangeValues() {
    store("Document/panelPreviewsSize", 170);
    store("Document/zoomStep", 0.29f);
    SettingsDialog dialog;
    dialog.findChild<QPushButton *>("applyButton")->click();
    QCOMPARE(settings->panelPreviewsSize(), 170);
    QCOMPARE(settings->zoomStep(), 0.29f);
}

TG_BEHAVIOR_TEST_MAIN(SettingBoundsAreSharedTest)

#include "test_setting_bounds_are_shared.moc"

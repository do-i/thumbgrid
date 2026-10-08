// The Resize dialog serves two flows: editing the open picture in place, and
// the grid's "write resized copies" flow, where one choice is applied to every
// selected image. Each flow must offer only the controls that mean something
// for it, and report the choice as the right per-image rule.
#include "support/thumbgrid_test_support.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>

#include "gui/dialogs/resizedialog.h"

class ResizeDialogModesTest : public QObject {
    Q_OBJECT

private slots:
    // first: runs against a fresh install's unset setting
    void qualityDefaultsToStandardBilinear();
    void pictureViewHidesCopyOnlyControls();
    void singleCopyKeepsExactSizeAvailable();
    void severalImagesTurnSizeIntoAnIndependentBox();
    void longEdgeIsReportedAndOnlyItsSpinBoxIsLive();
    void shrinkOnlyIsOnByDefaultAndCanBeTurnedOff();
    void chosenFilterIsReportedAndRemembered();
};

namespace {

template<typename T>
T *child(ResizeDialog &dialog, const char *name) {
    return dialog.findChild<T *>(name);
}

} // namespace

void ResizeDialogModesTest::qualityDefaultsToStandardBilinear() {
    // bilinear is what resize always used before the choice existed
    QCOMPARE(settings->resizeFilter(), QI_FILTER_BILINEAR);
    ResizeDialog dialog(QSize(400, 200));
    QCOMPARE(child<QLabel>(dialog, "label_4")->text(), QStringLiteral("Quality:"));
    auto *quality = child<QComboBox>(dialog, "comboBox");
    QCOMPARE(quality->currentData().toInt(), int(QI_FILTER_BILINEAR));
    QVERIFY(quality->currentText().startsWith("Standard"));
    QVERIFY(!quality->toolTip().isEmpty());
    QCOMPARE(dialog.spec().filter, QI_FILTER_BILINEAR);
}

void ResizeDialogModesTest::pictureViewHidesCopyOnlyControls() {
    ResizeDialog dialog(QSize(400, 200));
    dialog.show();
    QVERIFY(!child<QRadioButton>(dialog, "byLongEdge")->isVisible());
    QVERIFY(!child<QCheckBox>(dialog, "shrinkOnly")->isVisible());
    QVERIFY(child<QCheckBox>(dialog, "keepAspectRatio")->isVisible());

    // the in-place edit has no skip rule and keeps reporting a plain size
    QSignalSpy sizes(&dialog, &ResizeDialog::sizeSelected);
    QSignalSpy specs(&dialog, &ResizeDialog::specSelected);
    child<QDoubleSpinBox>(dialog, "percent")->setValue(50.0);
    QVERIFY(!dialog.spec().shrinkOnly);
    QTest::mouseClick(child<QPushButton>(dialog, "okButton"), Qt::LeftButton);
    QCOMPARE(sizes.count(), 1);
    QCOMPARE(sizes.first().first().toSize(), QSize(200, 100));
    QCOMPARE(specs.count(), 0);
}

void ResizeDialogModesTest::singleCopyKeepsExactSizeAvailable() {
    ResizeDialog dialog(QSize(400, 200));
    dialog.setCopyMode(1);
    dialog.show();
    QVERIFY(dialog.windowTitle().contains("copy"));
    QVERIFY(child<QRadioButton>(dialog, "byLongEdge")->isVisible());
    QVERIFY(child<QCheckBox>(dialog, "keepAspectRatio")->isVisible());

    child<QRadioButton>(dialog, "byAbsoluteSize")->setChecked(true);
    child<QCheckBox>(dialog, "keepAspectRatio")->setChecked(false);
    child<QSpinBox>(dialog, "width")->setValue(300);
    child<QSpinBox>(dialog, "height")->setValue(50);
    ResizeSpec spec = dialog.spec();
    QCOMPARE(spec.mode, ResizeSpec::Exact);
    QCOMPARE(spec.size, QSize(300, 50));

    // copy mode reports the rule even when the seed image would be unchanged
    QSignalSpy specs(&dialog, &ResizeDialog::specSelected);
    child<QRadioButton>(dialog, "byPercentage")->setChecked(true);
    child<QDoubleSpinBox>(dialog, "percent")->setValue(100.0);
    QTest::mouseClick(child<QPushButton>(dialog, "okButton"), Qt::LeftButton);
    QCOMPARE(specs.count(), 1);
}

void ResizeDialogModesTest::severalImagesTurnSizeIntoAnIndependentBox() {
    ResizeDialog dialog(QSize(400, 200));
    dialog.setCopyMode(3);
    dialog.show();
    QVERIFY(dialog.windowTitle().contains("3"));
    // an exact size would stretch the images whose shape differs from the seed
    QVERIFY(!child<QCheckBox>(dialog, "keepAspectRatio")->isVisible());
    QVERIFY(!child<QPushButton>(dialog, "fillDesktopButton")->isVisible());

    // percentage mode forces aspect on; switching to the box must undo that
    child<QRadioButton>(dialog, "byAbsoluteSize")->setChecked(true);
    child<QSpinBox>(dialog, "width")->setValue(300);
    QCOMPARE(child<QSpinBox>(dialog, "height")->value(), 200); // not linked
    child<QSpinBox>(dialog, "height")->setValue(300);
    ResizeSpec spec = dialog.spec();
    QCOMPARE(spec.mode, ResizeSpec::FitWithin);
    QCOMPARE(spec.size, QSize(300, 300));

    // a common size is the box itself, not the seed image fitted into it
    child<QComboBox>(dialog, "resComboBox")->setCurrentIndex(5); // 1920 x 1080
    QCOMPARE(dialog.spec().size, QSize(1920, 1080));
}

void ResizeDialogModesTest::longEdgeIsReportedAndOnlyItsSpinBoxIsLive() {
    ResizeDialog dialog(QSize(400, 200));
    dialog.setCopyMode(2);
    dialog.show();
    auto *longEdge = child<QSpinBox>(dialog, "longEdge");
    QCOMPARE(longEdge->value(), 400); // seeded from the seed's longer side
    QVERIFY(!longEdge->isEnabled());

    child<QRadioButton>(dialog, "byLongEdge")->setChecked(true);
    QVERIFY(longEdge->isEnabled());
    QVERIFY(!child<QDoubleSpinBox>(dialog, "percent")->isEnabled());
    QVERIFY(!child<QSpinBox>(dialog, "width")->isEnabled());
    longEdge->setValue(128);
    ResizeSpec spec = dialog.spec();
    QCOMPARE(spec.mode, ResizeSpec::LongEdge);
    QCOMPARE(spec.longEdge, 128);

    // leaving the mode disables it again, whichever radio is picked next
    child<QRadioButton>(dialog, "byPercentage")->setChecked(true);
    QVERIFY(!longEdge->isEnabled());
    QVERIFY(child<QDoubleSpinBox>(dialog, "percent")->isEnabled());
}

void ResizeDialogModesTest::shrinkOnlyIsOnByDefaultAndCanBeTurnedOff() {
    ResizeDialog dialog(QSize(400, 200));
    dialog.setCopyMode(2);
    QVERIFY(dialog.spec().shrinkOnly);
    child<QCheckBox>(dialog, "shrinkOnly")->setChecked(false);
    QVERIFY(!dialog.spec().shrinkOnly);
}

void ResizeDialogModesTest::chosenFilterIsReportedAndRemembered() {
    settings->setResizeFilter(QI_FILTER_BILINEAR);
    {
        ResizeDialog dialog(QSize(400, 200));
        auto *filter = child<QComboBox>(dialog, "comboBox");
        QVERIFY2(filter->isEnabled(), "The filter choice should be live, not a placeholder.");
        QCOMPARE(filter->currentData().toInt(), int(QI_FILTER_BILINEAR));
        filter->setCurrentIndex(filter->findData(QI_FILTER_NEAREST));
        QCOMPARE(dialog.spec().filter, QI_FILTER_NEAREST);
        QTest::mouseClick(child<QPushButton>(dialog, "okButton"), Qt::LeftButton);
    }
    QCOMPARE(settings->resizeFilter(), QI_FILTER_NEAREST);
    ResizeDialog reopened(QSize(400, 200));
    QCOMPARE(child<QComboBox>(reopened, "comboBox")->currentData().toInt(), int(QI_FILTER_NEAREST));
}

TG_BEHAVIOR_TEST_MAIN(ResizeDialogModesTest)

#include "test_resize_dialog_modes.moc"

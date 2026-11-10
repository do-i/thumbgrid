#include "support/thumbgrid_test_support.h"

#include "constants.h"
#include "gui/dialogs/settingsdialog.h"
#include "gui/viewers/textviewer.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>

// The text viewer shows the first 1 MiB of a file unless the user sets another
// size limit in Settings; the viewer picks a changed limit up without a restart.
class TextPreviewSizeLimitTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void limitIsOneMiBByDefault();
    void pickedLimitRoundTripsAndClamps();
    void settingsDialogSavesTheLimit();
    void viewerCutsTheFileAtTheLimit();
};

using namespace tg::bounds;

void TextPreviewSizeLimitTest::init() {
    QSettings conf;
    conf.remove("Advanced/textPreviewLimitKiB");
    conf.sync();
}

void TextPreviewSizeLimitTest::limitIsOneMiBByDefault() {
    QCOMPARE(settings->textPreviewLimitKiB(), 1024);
}

void TextPreviewSizeLimitTest::pickedLimitRoundTripsAndClamps() {
    QSettings conf;
    conf.setValue("Advanced/textPreviewLimitKiB", 999999);
    conf.sync();
    QCOMPARE(settings->textPreviewLimitKiB(), kTextPreviewLimitMaxKiB);
    conf.setValue("Advanced/textPreviewLimitKiB", 1);
    conf.sync();
    QCOMPARE(settings->textPreviewLimitKiB(), kTextPreviewLimitMinKiB);
    settings->setTextPreviewLimitKiB(4096);
    QCOMPARE(settings->textPreviewLimitKiB(), 4096);
}

void TextPreviewSizeLimitTest::settingsDialogSavesTheLimit() {
    settings->setTextPreviewLimitKiB(2048);
    SettingsDialog dialog;
    auto spin = dialog.findChild<QSpinBox *>("textPreviewLimitSpinBox");
    QVERIFY(spin);
    QCOMPARE(spin->minimum(), kTextPreviewLimitMinKiB);
    QCOMPARE(spin->maximum(), kTextPreviewLimitMaxKiB);
    QCOMPARE(spin->value(), 2048);
    spin->setValue(512);
    dialog.findChild<QPushButton *>("applyButton")->click();
    QCOMPARE(settings->textPreviewLimitKiB(), 512);
}

void TextPreviewSizeLimitTest::viewerCutsTheFileAtTheLimit() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("big.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray(200 * 1024, 'a'));
    file.close();

    settings->setTextPreviewLimitKiB(64);
    TextViewer viewer;
    auto edit = viewer.findChild<QPlainTextEdit *>();
    auto notice = viewer.findChild<QLabel *>();
    QVERIFY(edit && notice);
    QVERIFY(viewer.showFile(path));
    QCOMPARE(edit->toPlainText().size(), 64 * 1024);
    QVERIFY(!notice->isHidden());

    // raised at runtime: the next file shown is no longer cut
    settings->setTextPreviewLimitKiB(1024);
    emit settings->settingsChanged();
    QVERIFY(viewer.showFile(path));
    QCOMPARE(edit->toPlainText().size(), 200 * 1024);
    QVERIFY(notice->isHidden());
}

TG_BEHAVIOR_TEST_MAIN(TextPreviewSizeLimitTest)

#include "test_text_preview_size_limit.moc"

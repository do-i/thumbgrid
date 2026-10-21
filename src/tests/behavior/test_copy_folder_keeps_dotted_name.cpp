#include "support/thumbgrid_test_support.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include "components/fileoperationscontroller.h"

// A recursive copy built the destination folder from QFileInfo::baseName(),
// which drops everything from the first dot: "photos.2024" arrived as "photos",
// merging into any folder already called that.

class CopyFolderKeepsDottedNameTest : public QObject {
    Q_OBJECT
private slots:
    void aCopiedFolderKeepsItsFullName();
};

void CopyFolderKeepsDottedNameTest::aCopiedFolderKeepsItsFullName() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    QDir root(tmp.path());
    QVERIFY(root.mkpath("photos.2024.v2/inner"));
    QVERIFY(root.mkpath("dest"));
    QFile payload(root.filePath("photos.2024.v2/inner/a.txt"));
    QVERIFY(payload.open(QIODevice::WriteOnly));
    payload.write("payload");
    payload.close();

    MW *window = new MW();
    FileOperationsController ops(window);
    QVERIFY(ops.interactiveCopy({root.filePath("photos.2024.v2")}, root.filePath("dest")));

    QVERIFY2(QFileInfo(root.filePath("dest/photos.2024.v2/inner/a.txt")).isFile(),
             "the copy must land under the folder's full name");
    QVERIFY2(!QFileInfo::exists(root.filePath("dest/photos")),
             "the name must not be cut at the first dot");
}

TG_BEHAVIOR_TEST_MAIN(CopyFolderKeepsDottedNameTest)

#include "test_copy_folder_keeps_dotted_name.moc"

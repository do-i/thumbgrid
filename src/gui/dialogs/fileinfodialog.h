#pragma once

#include <QDialog>
#include <QList>

class QTabWidget;
class QVBoxLayout;
class QWidget;
class QLabel;
class EntryInfoItem;

// Non-modal inspector window showing metadata for the current selection.
// Core owns a single instance and retargets it live as the document-view
// image or the grid selection changes (docs/009 §B1). The EXIF tab is
// enabled only when DocumentInfo::getExifTags() returns rows (docs/009 §B2).
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileInfoDialog(QWidget *parent = nullptr);

    // Repopulates the General and EXIF tabs for path. An empty or missing
    // path drops to a "No selection" placeholder and disables the EXIF tab;
    // a folder, a non-image, or an image without EXIF tags also disables it
    // (never an enabled-but-empty tab).
    void setTarget(const QString &path);
    void clearTarget();

    // test access
    QTabWidget *tabs() { return mTabs; }

private:
    void populateGeneralTab(const QString &path);
    void populateExifTab(const QString &path);
    void clearGeneralRows();
    void addGeneralRow(const QString &name, const QString &value);
    void clearExifRows();
    void addExifRow(const QString &name, const QString &value);
    static QString permissionsString(const QString &path);

    QTabWidget *mTabs = nullptr;
    QWidget *mGeneralTab = nullptr;
    QWidget *mExifTab = nullptr;
    QLabel *mPlaceholder = nullptr;
    QWidget *mRowsContainer = nullptr;
    QVBoxLayout *mRowsLayout = nullptr;
    QList<EntryInfoItem *> mRows;
    QWidget *mExifRowsContainer = nullptr;
    QVBoxLayout *mExifRowsLayout = nullptr;
    QList<EntryInfoItem *> mExifRows;
    QString mTargetPath;
};

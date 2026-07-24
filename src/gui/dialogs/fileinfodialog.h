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
// image or the grid selection changes (docs/009 §B1). The EXIF tab is a
// disabled placeholder here; B2 populates and enables it.
class FileInfoDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileInfoDialog(QWidget *parent = nullptr);

    // Repopulates the General tab for path. An empty or missing path drops
    // to a "No selection" placeholder; either way the EXIF tab stays disabled
    // until B2 wires it up.
    void setTarget(const QString &path);
    void clearTarget();

    // test access
    QTabWidget *tabs() { return mTabs; }

private:
    void populateGeneralTab(const QString &path);
    void populateExifTab(const QString &path); // B2 fills this in
    void clearGeneralRows();
    void addGeneralRow(const QString &name, const QString &value);
    static QString permissionsString(const QString &path);

    QTabWidget *mTabs = nullptr;
    QWidget *mExifTab = nullptr;
    QLabel *mPlaceholder = nullptr;
    QWidget *mRowsContainer = nullptr;
    QVBoxLayout *mRowsLayout = nullptr;
    QList<EntryInfoItem *> mRows;
    QString mTargetPath;
};

#pragma once

#include <QDialog>
#include <QMap>
#include <QString>

class QLineEdit;
class QLabel;

// Small modal form for the handful of text-valued Exif tags that can be edited
// safely (DocumentInfo::editableTagKeys()).
//
// Deliberately a separate dialog rather than inline editing in the File info
// window's EXIF tab: that tab lists Exif, Iptc *and* Xmp with exiv2's
// interpreted values, almost none of which can be written back, so making some
// rows editable and not others would be the confusing option. A form also gives
// an unambiguous Save/Cancel, which an inspector window does not have.
//
// The dialog only collects and validates values - Core does the writing, so the
// confirmation, reload and error reporting stay in one place.
class MetadataEditDialog : public QDialog {
    Q_OBJECT
public:
    // current maps exiv2 key -> existing value; keys absent from it start blank.
    MetadataEditDialog(const QString &fileName,
                       const QMap<QString, QString> &current,
                       QWidget *parent = nullptr);

    // Only the fields the user actually changed. An emptied field is present
    // with an empty value, which DocumentInfo treats as "remove this tag";
    // untouched fields are absent, so a save never rewrites what it did not
    // need to touch.
    QMap<QString, QString> editedValues() const;

    // test access
    QLineEdit *fieldFor(const QString &key) const { return mFields.value(key, nullptr); }

private slots:
    // Blocks accept() on a malformed Date/Time rather than letting the write
    // fail later with a less specific message.
    void onAccept();

private:
    QMap<QString, QLineEdit *> mFields;
    QMap<QString, QString> mOriginal;
    QLabel *mError = nullptr;
};

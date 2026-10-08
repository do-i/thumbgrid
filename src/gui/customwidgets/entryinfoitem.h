#ifndef ENTRYINFOITEM_H
#define ENTRYINFOITEM_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QStyleOption>
#include <QPainter>
#include <QDebug>

class EntryInfoItem : public QWidget
{
    Q_OBJECT
public:
    explicit EntryInfoItem(QWidget *parent = nullptr);
    void setInfo(QString _name, QString _value);

    // Swaps the value label for an in-place editor. The editor is frameless
    // until hovered/focused (see the FileInfoDialog QSS rules), so an editable
    // row keeps the shape of a read-only one - the user clicks the value and
    // types, with no separate edit mode to enter.
    void setEditable(bool _editable);
    // Makes the *name* cell an editor too, which turns the row into a grid row:
    // a custom XMP property is identified by its key, so the key has to be
    // typeable. Rows whose name is fixed furniture (every curated row) leave
    // this off.
    void setNameEditable(bool _editable);
    // A short multi-line value editor, one item per line - the only unambiguous
    // way to edit an XMP array, since exiv2's displayed form joins items with
    // ", " and an item may itself contain a comma.
    void setMultiLine(bool _multiLine);
    // A trailing "x" that removes the whole row. Revealed by the tab's remove
    // toggle rather than shown always, so an ordinary read of the tab carries no
    // delete buttons.
    void setRemovable(bool _removable);
    void setValuePlaceholder(const QString &text);
    void setNamePlaceholder(const QString &text);
    // Widens the name column. The default suits the short human labels the
    // General and EXIF tabs use; a full exiv2 XMP key ("Xmp.photoshop.Headline")
    // does not fit in it, and a clipped key is not identifiable.
    void setNameWidth(int width);
    // A third cell, revealed only when a grid row names an XMP prefix the file
    // does not already declare: writing such a prefix throws unless its
    // namespace URI is registered first, so the row asks for one rather than
    // failing at the write.
    void setExtraVisible(bool visible);
    void setExtraPlaceholder(const QString &text);
    QString currentExtra() const;
    QLineEdit *extraEditor() { return extraVisible ? &extraEdit : nullptr; }
    // Greys the row out and refuses edits: used for a value too long to show in
    // full, which must never be committed back or the elision would truncate the
    // file's real data.
    void setValueReadOnly(bool readOnly);

    // Reports the editor's current text as an edit if it differs from the value
    // the row was given. Idempotent: a second call reports nothing. A grid row
    // reports name and value together as one rowEdited(), because committing
    // per field would write a key-only property on the way to a complete one.
    void commitEdit();

    // test access; null while the row is read-only
    QLineEdit *valueEditor() { return (editable && !multiLine) ? &valueEdit : nullptr; }
    QLineEdit *nameEditor() { return nameEditable ? &nameEdit : nullptr; }
    QPlainTextEdit *multiLineEditor() { return multiLine ? &valueMultiEdit : nullptr; }
    QPushButton *removeButton() { return removable ? &removeBtn : nullptr; }
    QString currentName() const;
    QString currentValue() const;
    // True when neither cell holds anything - the trailing affordance row, which
    // is never written.
    bool isBlank() const;

signals:
    // One signal per committed change (Enter or focus-out), never per keystroke,
    // and never when the text came back unchanged.
    void valueEdited(const QString &newValue);
    // Grid rows only: name and value as one commit.
    void rowEdited(const QString &newName, const QString &newValue);
    void removeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateElidedText();
    QString editorText() const;

    QString name;
    QString value;
    // The last committed namespace URI, so filling that cell in on its own is a
    // change worth reporting - the row is invalid until it is supplied, and
    // comparing only name and value would swallow the very edit that fixes it.
    QString extra;
    bool editable = false;
    bool nameEditable = false;
    bool multiLine = false;
    bool removable = false;
    bool extraVisible = false;
    QHBoxLayout layout;
    QLabel nameLabel, valueLabel;
    QLineEdit nameEdit;
    QLineEdit valueEdit;
    QLineEdit extraEdit;
    QPlainTextEdit valueMultiEdit;
    QPushButton removeBtn;
};

#endif // ENTRYINFOITEM_H

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

    // in-place editor, frameless until hovered/focused, so editable rows look like read-only ones
    void setEditable(bool _editable);
    // editable name cell makes a grid row (custom XMP properties are identified by key)
    void setNameEditable(bool _editable);
    // one item per line: XMP array items may contain commas
    void setMultiLine(bool _multiLine);
    // trailing "x" removing the row, revealed by the tab's remove toggle
    void setRemovable(bool _removable);
    void setValuePlaceholder(const QString &text);
    void setNamePlaceholder(const QString &text);
    // full exiv2 keys need a wider name column
    void setNameWidth(int width);
    // third cell for the namespace URI of an undeclared prefix
    void setExtraVisible(bool visible);
    void setExtraPlaceholder(const QString &text);
    QString currentExtra() const;
    QLineEdit *extraEditor() { return extraVisible ? &extraEdit : nullptr; }
    // for values too long to show: committing the elided text would truncate the file's data
    void setValueReadOnly(bool readOnly);

    // Reports a changed value once (idempotent). Grid rows report name and value together,
    // so no key-only property is written on the way.
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
    // last committed namespace URI, so supplying it alone counts as an edit
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

#ifndef ENTRYINFOITEM_H
#define ENTRYINFOITEM_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
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
    void setValuePlaceholder(const QString &text);
    // Reports the editor's current text as an edit if it differs from the value
    // the row was given. Idempotent: a second call reports nothing.
    void commitEdit();

    // test access; null while the row is read-only
    QLineEdit *valueEditor() { return editable ? &valueEdit : nullptr; }

signals:
    // One signal per committed change (Enter or focus-out), never per keystroke,
    // and never when the text came back unchanged.
    void valueEdited(const QString &newValue);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateElidedText();

    QString name;
    QString value;
    bool editable = false;
    QHBoxLayout layout;
    QLabel nameLabel, valueLabel;
    QLineEdit valueEdit;
};

#endif // ENTRYINFOITEM_H

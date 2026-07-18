#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;

// Frameless, translucent, rounded confirmation / message dialog that is fully
// styled by the active color scheme (see the CustomMessageBox rules in
// style-template.qss). Replaces stock QMessageBox for in-app prompts so they
// match the rest of the custom-drawn UI instead of showing a native OS dialog.
class CustomMessageBox : public QDialog {
    Q_OBJECT
public:
    explicit CustomMessageBox(QWidget *parent = nullptr);
    ~CustomMessageBox() override;

    void setTitle(const QString& title);
    void setText(const QString& text);
    // Adds a single-line text field to the body and returns it. Its content
    // is selected and focused so the user can type/replace immediately.
    QLineEdit* addInput(const QString& initialValue = QString());
    // role == true accepts the dialog, false rejects it. The first button
    // added with makeDefault triggers on Enter and receives initial focus.
    QPushButton* addButton(const QString& text, bool acceptRole, bool makeDefault = false);

    // Yes / No confirmation. Returns true when the accept button was chosen.
    static bool confirm(QWidget *parent, const QString& title, const QString& text,
                        const QString& acceptText = QObject::tr("Yes"),
                        const QString& rejectText = QObject::tr("No"));
    // Single-button notice (info / error). Blocks until dismissed.
    static void message(QWidget *parent, const QString& title, const QString& text,
                        const QString& buttonText = QObject::tr("OK"));
    // Themed replacement for QInputDialog::getText. Returns the entered text;
    // when *ok is provided it reports whether the accept button was chosen.
    static QString getText(QWidget *parent, const QString& title, const QString& label,
                           const QString& initialValue = QString(), bool *ok = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void centerOnParent();

    QLabel      *titleLabel;
    QLabel      *textLabel;
    QLineEdit   *inputField;
    QVBoxLayout *rootLayout;
    QHBoxLayout *buttonLayout;
};

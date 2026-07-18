#include "custommessagebox.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QScreen>

CustomMessageBox::CustomMessageBox(QWidget *parent)
    : QDialog(parent),
      titleLabel(new QLabel(this)),
      textLabel(new QLabel(this)),
      inputField(nullptr),
      rootLayout(new QVBoxLayout(this)),
      buttonLayout(new QHBoxLayout())
{
    // Frameless + translucent so the rounded themed body (drawn in paintEvent
    // via the widget stylesheet) is what the user sees, not an OS window frame.
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setModal(true);

    titleLabel->setAccessibleName(QStringLiteral("DialogTitle"));
    titleLabel->setWordWrap(true);

    textLabel->setAccessibleName(QStringLiteral("DialogText"));
    textLabel->setWordWrap(true);
    textLabel->setMaximumWidth(360);

    rootLayout->setContentsMargins(20, 18, 20, 16);
    rootLayout->setSpacing(12);
    rootLayout->addWidget(titleLabel);
    rootLayout->addWidget(textLabel);

    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch(1);
    rootLayout->addLayout(buttonLayout);
}

CustomMessageBox::~CustomMessageBox() = default;

void CustomMessageBox::setTitle(const QString& title) {
    titleLabel->setText(title);
    titleLabel->setVisible(!title.isEmpty());
}

void CustomMessageBox::setText(const QString& text) {
    textLabel->setText(text);
    textLabel->setVisible(!text.isEmpty());
}

QLineEdit* CustomMessageBox::addInput(const QString& initialValue) {
    if(!inputField) {
        inputField = new QLineEdit(this);
        // Sit above the button row (last item in the root layout).
        rootLayout->insertWidget(rootLayout->count() - 1, inputField);
        // Enter in the field accepts the dialog.
        connect(inputField, &QLineEdit::returnPressed, this, &QDialog::accept);
    }
    inputField->setText(initialValue);
    inputField->selectAll();
    inputField->setFocus(Qt::OtherFocusReason);
    return inputField;
}

QPushButton* CustomMessageBox::addButton(const QString& text, bool acceptRole, bool makeDefault) {
    auto *button = new QPushButton(text, this);
    button->setCursor(Qt::PointingHandCursor);
    if(acceptRole)
        connect(button, &QPushButton::clicked, this, &QDialog::accept);
    else
        connect(button, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(button);
    if(makeDefault) {
        button->setDefault(true);
        button->setFocus(Qt::OtherFocusReason);
    }
    return button;
}

bool CustomMessageBox::confirm(QWidget *parent, const QString& title, const QString& text,
                               const QString& acceptText, const QString& rejectText) {
    CustomMessageBox box(parent);
    box.setTitle(title);
    box.setText(text);
    box.addButton(rejectText, false);
    box.addButton(acceptText, true, true);
    return box.exec() == QDialog::Accepted;
}

void CustomMessageBox::message(QWidget *parent, const QString& title, const QString& text,
                               const QString& buttonText) {
    CustomMessageBox box(parent);
    box.setTitle(title);
    box.setText(text);
    box.addButton(buttonText, true, true);
    box.exec();
}

QString CustomMessageBox::getText(QWidget *parent, const QString& title, const QString& label,
                                  const QString& initialValue, bool *ok) {
    CustomMessageBox box(parent);
    box.setTitle(title);
    box.setText(label);
    box.addButton(tr("Cancel"), false);
    box.addButton(tr("OK"), true, true);
    QLineEdit *field = box.addInput(initialValue); // added last so it keeps focus
    const bool accepted = box.exec() == QDialog::Accepted;
    if(ok)
        *ok = accepted;
    return accepted ? field->text() : QString();
}

void CustomMessageBox::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    // Let the stylesheet paint the rounded, themed background/border.
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CustomMessageBox::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    centerOnParent();
}

void CustomMessageBox::centerOnParent() {
    QWidget *ref = parentWidget();
    const QRect area = ref ? ref->frameGeometry()
                           : (screen() ? screen()->availableGeometry() : QRect());
    if(area.isNull())
        return;
    move(area.center() - QPoint(width() / 2, height() / 2));
}

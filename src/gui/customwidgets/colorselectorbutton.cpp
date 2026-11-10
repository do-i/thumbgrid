#include "colorselectorbutton.h"
#include "gui/dialogs/dialogbuttonrow.h"

#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <utility>

ColorSelectorButton::ColorSelectorButton(QWidget *parent) : ClickableLabel(parent) {
    connect(this, &ColorSelectorButton::clicked, this, &ColorSelectorButton::showColorSelector);
}

void ColorSelectorButton::setColor(QColor &newColor) {
    mColor = newColor;
    update();
}

void ColorSelectorButton::setDescription(QString text) {
    this->mDescription = std::move(text);
}

QColor ColorSelectorButton::color() {
    return mColor;
}

void ColorSelectorButton::showColorSelector() {
    // getColor() only reports the final pick; embed a buttonless QColorDialog to support live Apply
    const QColor initialColor = mColor;
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("ColorPickerDialog"));
    dialog.setWindowTitle(mDescription);

    auto *picker = new QColorDialog(initialColor, &dialog);
    picker->setWindowFlags(Qt::Widget);
    picker->setOptions(QColorDialog::NoButtons | QColorDialog::DontUseNativeDialog);
    picker->setCurrentColor(initialColor);
    QTimer::singleShot(0, picker, [picker, initialColor]() {
        picker->setCurrentColor(initialColor);
    });
    // QColorDialog is itself a QDialog; if a key press makes it accept/reject,
    // forward that to the wrapper instead of it just hiding inside the layout
    connect(picker, &QDialog::finished, &dialog, &QDialog::done);

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(picker);
    auto *applyButton = DialogButtonRow::add(&dialog, layout, true).apply;

    QColor appliedColor; // invalid until the first Apply
    connect(applyButton, &QPushButton::clicked, this, [&]() {
        QColor newColor = picker->currentColor();
        // skip the app-wide re-theme when this exact color is already live
        if(newColor == (appliedColor.isValid() ? appliedColor : initialColor))
            return;
        appliedColor = newColor;
        mColor = newColor;
        update();
        emit colorApplied(mColor);
    });

    if(dialog.exec() == QDialog::Accepted) {
        mColor = picker->currentColor();
        update();
        if(mColor != (appliedColor.isValid() ? appliedColor : initialColor))
            emit colorApplied(mColor);
        emit colorChanged(mColor);
    } else if(appliedColor.isValid() && appliedColor != initialColor) {
        // cancel undoes any previews from this picker session
        mColor = initialColor;
        update();
        emit colorApplied(mColor);
    }
}

void ColorSelectorButton::paintEvent(QPaintEvent *e) {
    Q_UNUSED(e)

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if(!this->isEnabled())
        p.setOpacity(0.5f);
    p.setPen(QColor(40,40,40));
    p.drawRect(QRectF(0.5f, 0.5f, width() - 1.0f, height() - 1.0f));
    p.fillRect(rect().adjusted(2,2,-2,-2), mColor);
}

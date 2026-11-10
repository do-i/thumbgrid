#pragma once

class QBoxLayout;
class QDialog;
class QPushButton;

// QDialogButtonBox order follows the platform style, so dialogs lay out their
// own OK / Cancel (/ Apply) row; this keeps that row and its flags in one place.
namespace DialogButtonRow {

// Accent default on ok, hand cursor and no stock icons on every button.
void styleButtons(QPushButton *ok, QPushButton *cancel, QPushButton *apply = nullptr);

struct Buttons {
    QPushButton *ok;
    QPushButton *cancel;
    QPushButton *apply; // nullptr unless requested
};

// Appends a right-aligned OK, Cancel (, Apply) row to layout; OK accepts, Cancel rejects.
Buttons add(QDialog *dialog, QBoxLayout *layout, bool withApply = false);

} // namespace DialogButtonRow

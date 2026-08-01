#pragma once

#include <QDateTime>
#include <QWidget>

class QCalendarWidget;
class QTimeEdit;

// Calendar + clock drop-down for a date/time field. A Qt::Popup window rather
// than a dialog: it is an accessory to the field that opened it, so it closes on
// the first click outside and never takes a place in the window stack.
//
// It only picks a value - the field it was opened from decides what to do with
// it, which is what keeps typing into that field the primary way to set a date
// (the picker is the convenience, not the gate).
//
// Qt's own QDateTimeEdit(calendarPopup) was the alternative and was not used:
// it cannot represent "no value", and an absent Exif.Image.DateTime has to stay
// expressible - clearing the field is how the tag gets removed.
class DateTimePickerPopup : public QWidget {
    Q_OBJECT
public:
    explicit DateTimePickerPopup(QWidget *parent = nullptr);

    // Seeds the calendar and clock. An invalid value falls back to now, so the
    // popup always opens on something the user can adjust from.
    void setDateTime(const QDateTime &dateTime);
    QDateTime dateTime() const;

    // Opens directly under widget, pulled back on screen if that would run off
    // the edge.
    void popupUnder(QWidget *widget);

signals:
    // Emitted once, on Set (or a double-click in the calendar); picking is
    // deliberately explicit so a stray click on a day never rewrites the file.
    void dateTimePicked(const QDateTime &dateTime);

private:
    void applyThemeColors();

    QCalendarWidget *mCalendar = nullptr;
    QTimeEdit *mTime = nullptr;
};

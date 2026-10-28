#pragma once

#include <QDateTime>
#include <QWidget>

class QCalendarWidget;
class QTimeEdit;

// Calendar + clock popup for a date/time field; closes on an outside click. The field decides what to do with the pick.
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

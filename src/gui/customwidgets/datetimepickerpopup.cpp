#include "datetimepickerpopup.h"

#include <QCalendarWidget>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QScreen>
#include <QTextCharFormat>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "settings.h"

DateTimePickerPopup::DateTimePickerPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("dateTimePickerPopup"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    mCalendar = new QCalendarWidget(this);
    mCalendar->setGridVisible(false);
    mCalendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    mCalendar->setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    // The nav bar's month/year buttons open menus of their own; leaving them on
    // keeps a jump to a distant year (a scanned photo's date) one click away.
    mCalendar->setNavigationBarVisible(true);
    layout->addWidget(mCalendar);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(6);
    mTime = new QTimeEdit(this);
    mTime->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    mTime->setObjectName(QStringLiteral("dateTimePickerTime"));
    bottomRow->addWidget(mTime);
    bottomRow->addStretch(1);

    auto *nowButton = new QPushButton(tr("Now"), this);
    nowButton->setObjectName(QStringLiteral("dateTimePickerNowButton"));
    nowButton->setAutoDefault(false);
    connect(nowButton, &QPushButton::clicked, this,
            [this]() { setDateTime(QDateTime::currentDateTime()); });
    bottomRow->addWidget(nowButton);

    auto *setButton = new QPushButton(tr("Set"), this);
    setButton->setObjectName(QStringLiteral("dateTimePickerSetButton"));
    setButton->setAutoDefault(false);
    connect(setButton, &QPushButton::clicked, this, [this]() {
        const QDateTime picked = dateTime();
        close();
        emit dateTimePicked(picked);
    });
    bottomRow->addWidget(setButton);
    layout->addLayout(bottomRow);

    // A double-click on a day is the other unambiguous "this one" gesture; the
    // single click that selects a day only moves the selection.
    connect(mCalendar, &QCalendarWidget::activated, this, [this](const QDate &) {
        const QDateTime picked = dateTime();
        close();
        emit dateTimePicked(picked);
    });

    applyThemeColors();
    setDateTime(QDateTime::currentDateTime());
}

// QCalendarWidget paints its day grid through an internal item view, and its
// weekday/weekend header formats are QTextCharFormats - neither follows the
// app's stylesheet, so the active color scheme is pushed in as a palette (the
// same approach textviewer.cpp takes for QPlainTextEdit) with the header
// formats set explicitly on top. Without this the calendar renders in the
// native light palette inside a dark themed popup.
void DateTimePickerPopup::applyThemeColors() {
    const ColorScheme &colors = settings->colorScheme();

    QPalette pal = palette();
    pal.setColor(QPalette::Window, colors.widget);
    pal.setColor(QPalette::Base, colors.widget);
    pal.setColor(QPalette::AlternateBase, colors.widget);
    pal.setColor(QPalette::Text, colors.text_hc2);
    pal.setColor(QPalette::WindowText, colors.text_hc2);
    pal.setColor(QPalette::ButtonText, colors.text_hc2);
    pal.setColor(QPalette::Button, colors.button);
    pal.setColor(QPalette::Highlight, colors.accent);
    pal.setColor(QPalette::HighlightedText, colors.text_hc);
    setPalette(pal);
    setAutoFillBackground(true);

    // The weekday header row is drawn from a char format, not from the view's
    // palette or the stylesheet - left alone it keeps a light native band across
    // the top of an otherwise dark calendar.
    QTextCharFormat headerFormat;
    headerFormat.setForeground(colors.text_lc);
    headerFormat.setBackground(colors.widget);
    mCalendar->setHeaderTextFormat(headerFormat);
    // Weekends default to red, which reads as an error color against these
    // themes rather than as "Saturday".
    QTextCharFormat dayFormat;
    dayFormat.setForeground(colors.text_hc2);
    mCalendar->setWeekdayTextFormat(Qt::Saturday, dayFormat);
    mCalendar->setWeekdayTextFormat(Qt::Sunday, dayFormat);
}

void DateTimePickerPopup::setDateTime(const QDateTime &dateTime) {
    const QDateTime value = dateTime.isValid() ? dateTime : QDateTime::currentDateTime();
    mCalendar->setSelectedDate(value.date());
    mTime->setTime(value.time());
}

QDateTime DateTimePickerPopup::dateTime() const {
    return QDateTime(mCalendar->selectedDate(), mTime->time());
}

void DateTimePickerPopup::popupUnder(QWidget *widget) {
    adjustSize();
    QPoint topLeft = widget ? widget->mapToGlobal(QPoint(0, widget->height()))
                            : QCursor::pos();
    if(QScreen *screen = QGuiApplication::screenAt(topLeft) ? QGuiApplication::screenAt(topLeft)
                                                            : QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        topLeft.setX(qBound(available.left(), topLeft.x(), available.right() - width()));
        // Flip above the field when there is no room below, rather than letting
        // the calendar hang off the bottom of the screen.
        if(topLeft.y() + height() > available.bottom() && widget)
            topLeft.setY(widget->mapToGlobal(QPoint(0, 0)).y() - height());
    }
    move(topLeft);
    show();
}

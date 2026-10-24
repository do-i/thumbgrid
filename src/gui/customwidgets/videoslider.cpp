#include "videoslider.h"

#include <QPainter>
#include <QStylePainter>
#include <QToolTip>

namespace {
// handle sizes: editable, then the slimmer committed look
constexpr int EDIT_HEIGHT = 18, EDIT_SQUARE = 7, EDIT_TIP = 6;
constexpr int SLIM_HEIGHT = 12, SLIM_SQUARE = 3, SLIM_TIP = 4;
// extra room around a handle that still grabs it
constexpr int GRAB_MARGIN = 3;

// h:mm:ss.d / m:ss.d, for the time shown while a handle is dragged
QString clipTimeText(int ms) {
    const int tenths = (ms % 1000) / 100;
    int seconds = ms / 1000;
    const int hours = seconds / 3600;
    seconds -= hours * 3600;
    const int minutes = seconds / 60;
    seconds -= minutes * 60;
    QString text = QString("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QChar('0')).arg(tenths);
    if(hours)
        text = QString("%1:%2").arg(hours).arg(text.rightJustified(7, '0'));
    return text;
}
}

VideoSlider::VideoSlider(QWidget *parent) : QSlider(parent) {
    setMouseTracking(true);
}

void VideoSlider::setClip(int start, int end, bool editable) {
    if(start < 0 || end < start) {
        start = end = -1;
        editable = false;
    }
    if(start == mClipStart && end == mClipEnd && editable == mClipEditable)
        return;
    mClipStart = start;
    mClipEnd = end;
    mClipEditable = editable;
    if(!editable) {
        mDragHandle = -1;
        unsetCursor();
    }
    update();
}

void VideoSlider::setClipMinGap(int gap) {
    mClipMinGap = qMax(0, gap);
}

int VideoSlider::clipStart() const {
    return mClipStart;
}

int VideoSlider::clipEnd() const {
    return mClipEnd;
}

bool VideoSlider::clipEditable() const {
    return mClipEditable;
}

// the style's own math, so the clip lines up with the knob
int VideoSlider::xForValue(int value) const {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.sliderPosition = value;
    opt.sliderValue = value;
    return style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this).center().x();
}

int VideoSlider::valueForX(int x) const {
    const int x0 = xForValue(minimum());
    const int x1 = xForValue(maximum());
    if(x1 <= x0)
        return minimum();
    const qint64 span = qint64(maximum()) - minimum();
    const qint64 value = minimum() + (qint64(x - x0) * span + (x1 - x0) / 2) / (x1 - x0);
    return int(qBound(qint64(minimum()), value, qint64(maximum())));
}

// The handle's flat edge sits on its position and the body inside the clip.
// On a clip narrower than two handles they move apart instead, tip to tip
// over the middle of the clip, so they never overlap and stay grabbable.
QPolygonF VideoSlider::clipHandleShape(ClipHandle handle) const {
    if(mClipStart < 0)
        return QPolygonF();
    const int height = mClipEditable ? EDIT_HEIGHT : SLIM_HEIGHT;
    const qreal square = mClipEditable ? EDIT_SQUARE : SLIM_SQUARE;
    const qreal tip = mClipEditable ? EDIT_TIP : SLIM_TIP;
    const qreal xStart = xForValue(mClipStart);
    const qreal xEnd = xForValue(mClipEnd);
    const qreal centerX = (xStart + xEnd) / 2.0;
    const qreal top = (this->height() - height) / 2.0;
    const qreal bottom = top + height;
    const qreal middle = top + height / 2.0;
    // the start handle points right (+1), the end handle left (-1)
    const qreal dir = (handle == CLIP_HANDLE_START) ? 1.0 : -1.0;
    const qreal edge = (handle == CLIP_HANDLE_START) ? qMin(xStart, centerX - square - tip)
                                                     : qMax(xEnd, centerX + square + tip);
    QPolygonF shape;
    shape << QPointF(edge, top)
          << QPointF(edge + dir * square, top)
          << QPointF(edge + dir * (square + tip), middle)
          << QPointF(edge + dir * square, bottom)
          << QPointF(edge, bottom);
    return shape;
}

// With a clip: the groove, the groove outside the clip darkened, the knob,
// then the clip's handles on top.
void VideoSlider::paintEvent(QPaintEvent *event) {
    if(mClipStart < 0 || orientation() != Qt::Horizontal) {
        QSlider::paintEvent(event);
        return;
    }
    QStylePainter p(this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = QStyle::SC_SliderGroove;
    p.drawComplexControl(QStyle::CC_Slider, opt);

    const QColor accent = palette().color(QPalette::Highlight);
    const int xStart = xForValue(mClipStart);
    const int xEnd = xForValue(mClipEnd);
    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this)
                             .adjusted(0, -1, 0, 1);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor shade(0, 0, 0, 120);
    p.fillRect(QRect(QPoint(groove.left(), groove.top()), QPoint(xStart, groove.bottom())), shade);
    p.fillRect(QRect(QPoint(xEnd, groove.top()), QPoint(groove.right(), groove.bottom())), shade);
    if(mClipEditable) {
        QColor tint = accent;
        tint.setAlpha(45);
        const int bandHeight = EDIT_HEIGHT - 6;
        p.fillRect(QRect(xStart, (height() - bandHeight) / 2, xEnd - xStart, bandHeight), tint);
    }

    opt.subControls = QStyle::SC_SliderHandle;
    p.drawComplexControl(QStyle::CC_Slider, opt);

    QColor fill = accent;
    if(!mClipEditable)
        fill = fill.lighter(115);
    p.setPen(QPen(accent.darker(160), 1));
    p.setBrush(fill);
    p.drawPolygon(clipHandleShape(CLIP_HANDLE_START));
    p.drawPolygon(clipHandleShape(CLIP_HANDLE_END));
}

// the handle under pos (the nearer one where they are close), or -1
int VideoSlider::clipHandleAt(QPoint pos) const {
    if(!mClipEditable || mClipStart < 0)
        return -1;
    const QRectF startRect = clipHandleShape(CLIP_HANDLE_START).boundingRect()
                                 .adjusted(-GRAB_MARGIN, -GRAB_MARGIN, GRAB_MARGIN, GRAB_MARGIN);
    const QRectF endRect = clipHandleShape(CLIP_HANDLE_END).boundingRect()
                               .adjusted(-GRAB_MARGIN, -GRAB_MARGIN, GRAB_MARGIN, GRAB_MARGIN);
    const bool onStart = startRect.contains(pos);
    const bool onEnd = endRect.contains(pos);
    if(onStart && onEnd)
        return (pos.x() - xForValue(mClipStart) <= xForValue(mClipEnd) - pos.x()) ? CLIP_HANDLE_START
                                                                                  : CLIP_HANDLE_END;
    if(onStart)
        return CLIP_HANDLE_START;
    if(onEnd)
        return CLIP_HANDLE_END;
    return -1;
}

void VideoSlider::showClipTime(int value) {
    QToolTip::showText(mapToGlobal(QPoint(xForValue(value), -height())), clipTimeText(value), this);
}

void VideoSlider::mousePressEvent(QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return;
    event->accept();
    const int handle = clipHandleAt(event->pos());
    if(handle >= 0) {
        mDragHandle = handle;
        const int value = (handle == CLIP_HANDLE_START) ? mClipStart : mClipEnd;
        mDragOffset = event->pos().x() - xForValue(value);
        emit clipHandlePressed(handle);
        showClipTime(value);
        return;
    }
    setValueAtCursor(event->pos());
}

void VideoSlider::mouseMoveEvent(QMouseEvent *event) {
    if(mDragHandle >= 0 && (event->buttons() & Qt::LeftButton)) {
        event->accept();
        int value = valueForX(event->pos().x() - mDragOffset);
        // stop short of the other handle
        if(mDragHandle == CLIP_HANDLE_START)
            value = qBound(minimum(), value, qMax(minimum(), mClipEnd - mClipMinGap));
        else
            value = qBound(qMin(maximum(), mClipStart + mClipMinGap), value, maximum());
        int &edge = (mDragHandle == CLIP_HANDLE_START) ? mClipStart : mClipEnd;
        if(value != edge) {
            edge = value;
            update();
            emit clipHandleMoved(mDragHandle, value);
        }
        showClipTime(value);
        return;
    }
    if(event->buttons() & Qt::LeftButton) {
        event->accept();
        setValueAtCursor(event->pos());
        return;
    }
    if(clipHandleAt(event->pos()) >= 0)
        setCursor(Qt::SizeHorCursor);
    else
        unsetCursor();
}

void VideoSlider::mouseReleaseEvent(QMouseEvent *event) {
    if(mDragHandle >= 0 && event->button() == Qt::LeftButton) {
        const int handle = mDragHandle;
        mDragHandle = -1;
        QToolTip::hideText();
        emit clipHandleReleased(handle);
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

void VideoSlider::setValueAtCursor(QPoint pos) {
    if(orientation() == Qt::Vertical)
        setValue(minimum() + ((maximum() - minimum()) * (height() - pos.y())) / height() );
    else
        setValue(valueForX(pos.x()));
    emit sliderMovedX(value());
}

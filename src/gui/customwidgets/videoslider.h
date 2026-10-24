#pragma once

#include <QSlider>
#include <QMouseEvent>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QDebug>
#include <cstdint>

// The seek bar. It can also show a clip: two handles, a pentagon each - a
// square with a triangle on its inner side, so the start handle points right
// and the end handle left - with the bar outside the clip dimmed. While the
// clip is editable the handles can be dragged; they stop short of each other.
class VideoSlider : public QSlider {
    Q_OBJECT
public:
    enum ClipHandle : std::uint8_t {
        CLIP_HANDLE_START,
        CLIP_HANDLE_END
    };

    VideoSlider(QWidget *parent = nullptr);
    // start < 0 hides the clip
    void setClip(int start, int end, bool editable);
    // the closest the dragged handles may get
    void setClipMinGap(int gap);
    int clipStart() const;
    int clipEnd() const;
    bool clipEditable() const;
    // where the playhead knob is drawn for value
    int xForValue(int value) const;
    QPolygonF clipHandleShape(ClipHandle handle) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

signals:
    void sliderMovedX(int);
    void clipHandlePressed(int handle);
    void clipHandleMoved(int handle, int value);
    void clipHandleReleased(int handle);

private:
    void setValueAtCursor(QPoint pos);
    int valueForX(int x) const;
    int clipHandleAt(QPoint pos) const;
    void showClipTime(int value);

    int mClipStart = -1;
    int mClipEnd = -1;
    bool mClipEditable = false;
    int mClipMinGap = 0;
    int mDragHandle = -1;
    // from the grab point to the dragged handle's edge, so it doesn't jump
    int mDragOffset = 0;
};

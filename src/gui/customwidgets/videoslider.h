#pragma once

#include <QSlider>
#include <QMouseEvent>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QDebug>
#include <cstdint>

// Seek bar that can show a clip: two pentagon handles pointing inward, outside dimmed.
// Handles stop short of each other; a dragged handle stands in for the playhead.
class VideoSlider : public QSlider {
    Q_OBJECT
public:
    enum ClipHandle : std::uint8_t {
        CLIP_HANDLE_START,
        CLIP_HANDLE_END
    };

    VideoSlider(QWidget *parent = nullptr);
    // start < 0 hides the clip
    void setClip(int start, int end);
    // the closest the dragged handles may get
    void setClipMinGap(int gap);
    int clipStart() const;
    int clipEnd() const;
    // a handle is being dragged; the playhead knob is hidden meanwhile
    bool draggingClipHandle() const;
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

private:
    void setValueAtCursor(QPoint pos);
    int valueForX(int x) const;
    int clipHandleAt(QPoint pos) const;
    void showClipTime(int value);

    int mClipStart = -1;
    int mClipEnd = -1;
    int mClipMinGap = 0;
    int mDragHandle = -1;
    // from the grab point to the dragged handle's edge, so it doesn't jump
    int mDragOffset = 0;
};

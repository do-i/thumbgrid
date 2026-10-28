#pragma once

#include <QObject>
#include <cstdint>

// The clip of the shown video: a start..end range (in ms) set with two handles
// on the seek bar, and whether playback loops. The clip button shows the
// handles, and playback then stays inside the clip; the handles can be dragged
// whenever they are shown. Hiding them keeps the range, so showing them again
// brings it back. Without a clip, loop repeats the whole file. Everything
// resets when another file opens.
class VideoClip : public QObject {
    Q_OBJECT
public:
    enum State : std::uint8_t {
        CLIP_NONE,
        // a range is remembered but not used
        CLIP_HIDDEN,
        CLIP_ACTIVE
    };
    enum Handle : std::uint8_t {
        HANDLE_START,
        HANDLE_END
    };
    // the closest the handles may get, so they never cross
    static constexpr int MIN_GAP_MS = 100;
    // a position this close to the end counts as the end
    static constexpr int END_TOLERANCE_MS = 50;
    // the shortest clip a new one starts with
    static constexpr int NEW_CLIP_MIN_MS = 1000;

    explicit VideoClip(QObject *parent = nullptr);

    // another file: no clip, loop off
    void reset();
    void setDuration(int duration);
    int duration() const;

    // active -> hidden, hidden -> active. A new clip starts at pos (the
    // playhead) and ends halfway from there to the end of the video, but at
    // least NEW_CLIP_MIN_MS later; with less than that left after pos it
    // spans the whole video.
    void toggle(int pos);
    // forget the range
    void clear();
    // move a handle to pos ("[" / "]"), showing a hidden clip; without a clip
    // this makes one that runs from pos to the end of the file (or from the
    // start to pos)
    void markStart(int pos);
    void markEnd(int pos);
    // drag a shown handle; it stops short of the other one
    void moveHandle(Handle handle, int pos);
    void setLoop(bool mode);
    void toggleLoop();

    State state() const;
    bool isActive() const;
    // active or hidden: there is a range to show
    bool hasRange() const;
    int start() const;
    int end() const;
    bool loop() const;

    // where a seek to pos lands: inside the clip while it is active
    int seekTarget(int pos) const;
    // where play resumes from pos: -1 to resume in place, else the clip start
    // (pos is outside the clip, or at its end)
    int resumeTarget(int pos) const;
    // playback at pos has reached the end of an active clip that does not loop
    bool reachedEnd(int pos) const;

signals:
    void changed();

private:
    int gap() const;
    int boundedStart(int pos) const;
    int boundedEnd(int pos) const;

    State mState = CLIP_NONE;
    int mDuration = 0;
    int mStart = 0;
    int mEnd = 0;
    bool mLoop = false;
};

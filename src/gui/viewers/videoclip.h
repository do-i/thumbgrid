#pragma once

#include <QObject>
#include <cstdint>

// The shown video's clip: a start..end range (ms) and whether it loops. While shown, playback stays
// inside it; hiding keeps the range until another file opens.
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
    // a playhead this far from where the clip was hidden has moved
    static constexpr int PLAYHEAD_MOVED_MS = 50;

    explicit VideoClip(QObject *parent = nullptr);

    // another file: no clip, loop off
    void reset();
    void setDuration(int duration);
    int duration() const;

    // Toggle at playhead pos. A new clip ends halfway to the end, at least NEW_CLIP_MIN_MS later (whole video
    // if less remains). A hidden clip returns as it was unless the playhead moved: then its start moves there.
    void toggle(int pos);
    // forget the range
    void clear();
    // move a handle to pos ("[" / "]"), showing a hidden clip or creating one to the end (or from the start)
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
    // a new clip around pos
    void placeAt(int pos);
    int gap() const;
    int boundedStart(int pos) const;
    int boundedEnd(int pos) const;

    State mState = CLIP_NONE;
    int mDuration = 0;
    int mStart = 0;
    int mEnd = 0;
    bool mLoop = false;
    // the playhead when the clip was hidden
    int mHiddenAt = 0;
};

#pragma once

#include <QObject>
#include <cstdint>

// The clip of the shown video: a start..end range (in ms) set with two handles
// on the seek bar, and whether playback loops. Editing it is a mode: the
// handles can be dragged until the edit is committed (or cancelled), then the
// clip is active and playback stays inside it. Without a clip, loop repeats
// the whole file. Everything resets when another file opens.
class VideoClip : public QObject {
    Q_OBJECT
public:
    enum State : std::uint8_t {
        CLIP_NONE,
        CLIP_EDITING,
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

    explicit VideoClip(QObject *parent = nullptr);

    // another file: no clip, loop off
    void reset();
    void setDuration(int duration);
    int duration() const;

    // none -> editing (30% .. 70%), active -> editing, editing -> active
    void toggleEdit();
    void commitEdit();
    // back to how it was before editing started
    void cancelEdit();
    void clear();
    // move a handle to pos ("[" / "]"); without a clip this makes one that runs
    // from pos to the end of the file (or from the start to pos)
    void markStart(int pos);
    void markEnd(int pos);
    // drag a handle; it stops short of the other one
    void moveHandle(Handle handle, int pos);
    void setLoop(bool mode);
    void toggleLoop();

    State state() const;
    bool isEditing() const;
    bool isActive() const;
    // editing or active: the handles are shown
    bool hasRange() const;
    int start() const;
    int end() const;
    bool loop() const;
    Handle lastHandle() const;

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
    Handle mLastHandle = HANDLE_START;
    // the clip as it was when editing began, for cancelEdit()
    State mSavedState = CLIP_NONE;
    int mSavedStart = 0;
    int mSavedEnd = 0;
};

#include "videoclip.h"

#include <QtGlobal>

VideoClip::VideoClip(QObject *parent) : QObject(parent) {
}

void VideoClip::reset() {
    const bool wasSet = mState != CLIP_NONE || mLoop;
    mState = CLIP_NONE;
    mStart = mEnd = 0;
    mLoop = false;
    mLastHandle = HANDLE_START;
    if(wasSet)
        emit changed();
}

void VideoClip::setDuration(int duration) {
    if(duration == mDuration)
        return;
    mDuration = qMax(0, duration);
    // a clip of the old length means nothing for the new one
    if(mState != CLIP_NONE) {
        mState = CLIP_NONE;
        emit changed();
    }
}

int VideoClip::duration() const {
    return mDuration;
}

void VideoClip::toggleEdit() {
    if(mState == CLIP_EDITING) {
        commitEdit();
        return;
    }
    if(mDuration <= gap())
        return;
    mSavedState = mState;
    mSavedStart = mStart;
    mSavedEnd = mEnd;
    if(mState == CLIP_NONE) {
        mStart = mDuration * 3 / 10;
        mEnd = mDuration * 7 / 10;
        mLastHandle = HANDLE_START;
    }
    mState = CLIP_EDITING;
    emit changed();
}

void VideoClip::commitEdit() {
    if(mState != CLIP_EDITING)
        return;
    mState = CLIP_ACTIVE;
    emit changed();
}

void VideoClip::cancelEdit() {
    if(mState != CLIP_EDITING)
        return;
    mState = mSavedState;
    mStart = mSavedStart;
    mEnd = mSavedEnd;
    emit changed();
}

void VideoClip::clear() {
    if(mState == CLIP_NONE)
        return;
    mState = CLIP_NONE;
    emit changed();
}

void VideoClip::markStart(int pos) {
    if(mDuration <= gap())
        return;
    if(mState == CLIP_NONE) {
        mEnd = mDuration;
        mState = CLIP_ACTIVE;
    }
    mStart = boundedStart(pos);
    mLastHandle = HANDLE_START;
    emit changed();
}

void VideoClip::markEnd(int pos) {
    if(mDuration <= gap())
        return;
    if(mState == CLIP_NONE) {
        mStart = 0;
        mState = CLIP_ACTIVE;
    }
    mEnd = boundedEnd(pos);
    mLastHandle = HANDLE_END;
    emit changed();
}

void VideoClip::moveHandle(Handle handle, int pos) {
    if(mState == CLIP_NONE)
        return;
    mLastHandle = handle;
    const int bounded = (handle == HANDLE_START) ? boundedStart(pos) : boundedEnd(pos);
    int &value = (handle == HANDLE_START) ? mStart : mEnd;
    if(bounded == value)
        return;
    value = bounded;
    emit changed();
}

void VideoClip::setLoop(bool mode) {
    if(mode == mLoop)
        return;
    mLoop = mode;
    emit changed();
}

void VideoClip::toggleLoop() {
    setLoop(!mLoop);
}

VideoClip::State VideoClip::state() const {
    return mState;
}

bool VideoClip::isEditing() const {
    return mState == CLIP_EDITING;
}

bool VideoClip::isActive() const {
    return mState == CLIP_ACTIVE;
}

bool VideoClip::hasRange() const {
    return mState != CLIP_NONE;
}

int VideoClip::start() const {
    return mStart;
}

int VideoClip::end() const {
    return mEnd;
}

bool VideoClip::loop() const {
    return mLoop;
}

VideoClip::Handle VideoClip::lastHandle() const {
    return mLastHandle;
}

int VideoClip::seekTarget(int pos) const {
    if(mState != CLIP_ACTIVE)
        return pos;
    return qBound(mStart, pos, mEnd);
}

int VideoClip::resumeTarget(int pos) const {
    if(mState != CLIP_ACTIVE)
        return -1;
    if(pos < mStart || pos >= mEnd - END_TOLERANCE_MS)
        return mStart;
    return -1;
}

bool VideoClip::reachedEnd(int pos) const {
    return mState == CLIP_ACTIVE && !mLoop && pos >= mEnd - END_TOLERANCE_MS;
}

// a clip on a very short video may be shorter than MIN_GAP_MS
int VideoClip::gap() const {
    return qMin(MIN_GAP_MS, mDuration / 2);
}

int VideoClip::boundedStart(int pos) const {
    return qBound(0, pos, qMax(0, mEnd - gap()));
}

int VideoClip::boundedEnd(int pos) const {
    return qBound(qMin(mDuration, mStart + gap()), pos, mDuration);
}

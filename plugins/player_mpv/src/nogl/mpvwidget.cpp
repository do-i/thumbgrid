#include "mpvwidget.h"
#include <stdexcept>

static void wakeup(void *ctx) {
    QMetaObject::invokeMethod((MpvWidget*)ctx, "on_mpv_events", Qt::QueuedConnection);       
}

MpvWidget::MpvWidget(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
    mpv = mpv_create();
    if(!mpv)
        throw std::runtime_error("could not create mpv context");

    int64_t wid = winId();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);

    // mpv draws into our native window with its own video output. Try the GPU
    // VO first, then fall back to XVideo and plain X11 so playback still works
    // where OpenGL/Vulkan are unavailable (e.g. software rendering / VMs).
    mpv_set_option_string(mpv, "vo", "gpu,xv,x11");
    // local files only: no youtube-dl hook, no user/bundled Lua scripts
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "load-scripts", "no");

    this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // A native window gets the pointer's X events itself despite the attribute
    // above, and Qt drops a buttonless move at a widget without mouse tracking
    // instead of passing it on: the viewer never saw the mouse move over a
    // video, so a pointer it had auto-hidden never came back.
    setMouseTracking(true);
    mpv_set_option_string(mpv, "input-cursor", "no");   // no mouse handling
    mpv_set_option_string(mpv, "cursor-autohide", "no");// no cursor-autohide, we handle that

    mpv::qt::set_property(mpv, "hwdec", "auto-safe");

    //mpv::qt::set_property(mpv, "video-unscaled", "downscale-big");

    // Loop video
    setRepeat(true);
    // stay on the last frame at the end instead of unloading the file, so a
    // video that doesn't loop can be played again (eof-reached tells the end)
    mpv::qt::set_property(mpv, "keep-open", "yes");

    // Unmute
    setMuted(false);
    
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "eof-reached", MPV_FORMAT_FLAG);
    // Track the actual audio-output (system / per-app) volume so the UI can
    // reflect the real output level, not just mpv's internal software gain.
    mpv_observe_property(mpv, 0, "ao-volume", MPV_FORMAT_DOUBLE);
    // Tells when the audio output comes up or goes away, so mute can move
    // between the software mute and the audio output's own (see applyMute).
    mpv_observe_property(mpv, 0, "current-ao", MPV_FORMAT_STRING);
    mpv_set_wakeup_callback(mpv, wakeup, this);
    
    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");    
}

MpvWidget::~MpvWidget() {
    mpv_terminate_destroy(mpv);
}

void MpvWidget::command(const QVariant& params) {
    mpv::qt::command(mpv, params);
}

void MpvWidget::setProperty(const QString& name, const QVariant& value) {
    mpv::qt::set_property(mpv, name, value);
}

QVariant MpvWidget::getProperty(const QString &name) const {
    return mpv::qt::get_property(mpv, name);
}

void MpvWidget::setOption(const QString& name, const QVariant& value) {
    mpv::qt::set_property(mpv, name, value);
}

void MpvWidget::on_mpv_events() {
    // Process all events, until the event queue is empty.
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handle_mpv_event(event);
    }
}

void MpvWidget::handle_mpv_event(mpv_event *event) {
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = reinterpret_cast<mpv_event_property*>(event->data);
        if(strcmp(prop->name, "time-pos") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double time = *reinterpret_cast<double*>(prop->data);
                emit positionChanged(qRound(time * 1000));
            }
        } else if(strcmp(prop->name, "duration") == 0) {
            if(prop->format == MPV_FORMAT_DOUBLE) {
                double time = *reinterpret_cast<double*>(prop->data);
                emit durationChanged(qRound(time * 1000));
            } else if(prop->format == MPV_FORMAT_NONE) {
                emit playbackFinished();
            }
        } else if(strcmp(prop->name, "eof-reached") == 0) {
            if(prop->format == MPV_FORMAT_FLAG && *reinterpret_cast<int*>(prop->data))
                emit playbackFinished();
        } else if(strcmp(prop->name, "pause") == 0) {
            int mode = *reinterpret_cast<int*>(prop->data);
            emit videoPaused(mode == 1);
        } else if(strcmp(prop->name, "ao-volume") == 0) {
            // Only meaningful once the audio output is initialized; before that
            // the property is unavailable (MPV_FORMAT_NONE) and is ignored.
            if(prop->format == MPV_FORMAT_DOUBLE) {
                double vol = *reinterpret_cast<double*>(prop->data);
                emit volumeChanged(static_cast<int>(vol + 0.5));
            }
        } else if(strcmp(prop->name, "current-ao") == 0) {
            const bool active = (prop->format == MPV_FORMAT_STRING);
            if(active != mAudioOutputActive) {
                mAudioOutputActive = active;
                applyMute();
            }
        }
        break;
    }
    case MPV_EVENT_PLAYBACK_RESTART:
        // Fired once the (new) file's first frame is decoded and ready to be
        // shown. Used to reveal the player only when there is fresh content,
        // so the previous file's last frame doesn't flash on screen.
        emit playbackRestarted();
        break;
    default: ;
        // Ignore uninteresting or unknown events.
    }
}

void MpvWidget::setMuted(bool mode) {
    mMuted = mode;
    applyMute();
}

// The software `mute` only silences audio that hasn't been decoded yet, so what
// already sits in the audio buffers (~100 ms) keeps playing after muting. While
// the audio output exists, mute that instead (the system / per-app stream),
// which is immediate both ways; the software mute covers the time before it
// starts. The stream's mute is remembered by PulseAudio and restored with the
// next stream, so ours is imposed again every time the audio output comes up.
void MpvWidget::applyMute() {
    if(mAudioOutputActive && mpv::qt::set_property(mpv, "ao-mute", mMuted) >= 0) {
        mpv::qt::set_property(mpv, "mute", false);
        return;
    }
    mpv::qt::set_property(mpv, "mute", mMuted);
}

bool MpvWidget::muted() {
    return mMuted;
}

int MpvWidget::volume() {
    // ao-volume is the real audio-output (system / per-app) level. It is only
    // available while audio is playing; fall back to full volume otherwise.
    QVariant vol = mpv::qt::get_property_variant(mpv, "ao-volume");
    if(!vol.isValid())
        return 100;
    return static_cast<int>(vol.toDouble() + 0.5);
}

void MpvWidget::setVolume(int vol) {
    vol = qBound(0, vol, 100);
    mpv::qt::set_property_variant(mpv, "ao-volume", vol);
}

void MpvWidget::setPlaybackSpeed(double speed) {
    speed = qBound(0.25, speed, 4.0);
    mpv::qt::set_property_variant(mpv, "speed", speed);
}

void MpvWidget::setLoopAB(int startPosition, int endPosition) {
    if(startPosition < 0 || endPosition <= startPosition) {
        mpv::qt::set_property_variant(mpv, "ab-loop-a", "no");
        mpv::qt::set_property_variant(mpv, "ab-loop-b", "no");
        return;
    }
    mpv::qt::set_property_variant(mpv, "ab-loop-a", startPosition / 1000.0);
    mpv::qt::set_property_variant(mpv, "ab-loop-b", endPosition / 1000.0);
}

void MpvWidget::setRepeat(bool mode) {
    if(mode)
        mpv::qt::set_property(mpv, "loop-file", "inf");
    else
        mpv::qt::set_property(mpv, "loop-file", "no");
}

#include "mpvwidget.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

static void wakeup(void *ctx) {
    QMetaObject::invokeMethod((MpvWidget*)ctx, "on_mpv_events", Qt::QueuedConnection);
}

#ifndef PLAYER_MPV_USE_WID
static void *get_proc_address(void *, const char *name) {
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return nullptr;
    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}
#endif

MpvWidget::MpvWidget(QWidget *parent, Qt::WindowFlags f)
    : MpvWidgetBase(parent, f)
{
    mpv = mpv_create();
    if(!mpv)
        throw std::runtime_error("could not create mpv context");

#ifdef PLAYER_MPV_USE_WID
    int64_t wid = winId();
    mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);
#endif
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // local files only: no youtube-dl hook, no user/bundled Lua scripts
    mpv_set_option_string(mpv, "ytdl", "no");
    mpv_set_option_string(mpv, "load-scripts", "no");

#ifdef PLAYER_MPV_USE_WID
    // gpu first, then xv / x11 for software rendering and VMs
    mpv_set_option_string(mpv, "vo", "gpu,xv,x11");
    // a native child swallows hover moves without tracking; cursor autohide needs them
    setMouseTracking(true);
    mpv_set_option_string(mpv, "input-cursor", "no");
    mpv_set_option_string(mpv, "cursor-autohide", "no");
    mpv::qt::set_property(mpv, "hwdec", "auto-safe");
    observeProperties();
    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");
#else
    mpv_set_option_string(mpv, "vo", "libmpv");
    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");
    // hwdec surfaces this render path cannot map show up as a blank frame
    mpv::qt::set_property(mpv, "hwdec", "no");
    observeProperties();
#endif
}

void MpvWidget::observeProperties() {
    setRepeat(true);
    // stay on the last frame at the end so a non-looping video can be replayed
    mpv::qt::set_property(mpv, "keep-open", "yes");
    setMuted(false);

    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "eof-reached", MPV_FORMAT_FLAG);
    // real output (system / per-app) level, not mpv's software gain
    mpv_observe_property(mpv, 0, "ao-volume", MPV_FORMAT_DOUBLE);
    // audio output up/down moves mute between software and AO mute (applyMute)
    mpv_observe_property(mpv, 0, "current-ao", MPV_FORMAT_STRING);
    mpv_set_wakeup_callback(mpv, wakeup, this);
}

MpvWidget::~MpvWidget() {
#ifndef PLAYER_MPV_USE_WID
    makeCurrent();
    if (mpv_gl)
        mpv_render_context_free(mpv_gl);
#endif
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

#ifndef PLAYER_MPV_USE_WID
void MpvWidget::initializeGL() {
    mpv_opengl_init_params gl_init_params{get_proc_address, nullptr};
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    if(mpv_render_context_create(&mpv_gl, mpv, params) < 0)
        throw std::runtime_error("failed to initialize mpv GL context");
    mpv_render_context_set_update_callback(mpv_gl, MpvWidget::on_update, reinterpret_cast<void *>(this));
}

void MpvWidget::paintGL() {
    mpv_opengl_fbo mpfbo{static_cast<int>(defaultFramebufferObject()), width(), height(), 0};
    int flip_y{1};

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    mpv_render_context_render(mpv_gl, params);
}

// Qt skips update() for a minimized window, which makes mpv's render API time
// out and stutter, so draw by hand there.
void MpvWidget::maybeUpdate() {
    if(window()->isMinimized()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        doneCurrent();
    } else {
        update();
    }
}

void MpvWidget::on_update(void *ctx) {
    QMetaObject::invokeMethod((MpvWidget*)ctx, "maybeUpdate");
}
#endif

void MpvWidget::on_mpv_events() {
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
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
            // unavailable (MPV_FORMAT_NONE) until the audio output is up
            if(prop->format == MPV_FORMAT_DOUBLE)
                emit volumeChanged(static_cast<int>(std::lround(*reinterpret_cast<double*>(prop->data))));
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
        // the new file has fresh output; the player uses it to end the stale-frame cover
        emit playbackRestarted();
        break;
    default:
        break;
    }
}

void MpvWidget::setMuted(bool mode) {
    mMuted = mode;
    applyMute();
}

// Software mute leaves ~100 ms of buffered audio playing, so while the audio
// output exists mute that instead. PulseAudio restores a stream's mute, hence
// this is re-applied whenever the audio output comes up.
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
    // ao-volume only exists while audio plays
    QVariant vol = mpv::qt::get_property_variant(mpv, "ao-volume");
    if(!vol.isValid())
        return 100;
    return static_cast<int>(std::lround(vol.toDouble()));
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
    mpv::qt::set_property(mpv, "loop-file", mode ? "inf" : "no");
}

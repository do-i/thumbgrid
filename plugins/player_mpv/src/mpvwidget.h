#pragma once

#include <QtCore/QMetaObject>
#include <QWidget>
#include <mpv/client.h>
#include "qthelper.hpp"

#ifdef PLAYER_MPV_USE_WID
using MpvWidgetBase = QWidget;
#else
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <mpv/render_gl.h>
using MpvWidgetBase = QOpenGLWidget;
#endif

class MpvWidget Q_DECL_FINAL : public MpvWidgetBase {
    Q_OBJECT
public:
    MpvWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::Widget);
    ~MpvWidget() override;

    void command(const QVariant& params);
    void setOption(const QString &name, const QVariant &value);
    void setProperty(const QString& name, const QVariant& value);
    QVariant getProperty(const QString& name) const;
#ifndef PLAYER_MPV_USE_WID
    // https://github.com/gnome-mpv/gnome-mpv/issues/245
    int width() const {
        return static_cast<int>(QOpenGLWidget::width() * this->devicePixelRatioF());
    }
    int height() const {
        return static_cast<int>(QOpenGLWidget::height() * this->devicePixelRatioF());
    }
#endif
    void setMuted(bool mode);
    void setRepeat(bool mode);
    bool muted();
    int volume();
    void setVolume(int vol);
    void setPlaybackSpeed(double speed);
    void setLoopAB(int startPosition, int endPosition);
    void setHardwareDecoding(bool mode);

signals:
    void durationChanged(int value);
    void positionChanged(int value);
    void videoPaused(bool);
    void playbackFinished();
    void playbackRestarted();
    void volumeChanged(int value);

#ifndef PLAYER_MPV_USE_WID
protected:
    void initializeGL() override;
    void paintGL() override;
#endif

private slots:
    void on_mpv_events();
#ifndef PLAYER_MPV_USE_WID
    void maybeUpdate();
#endif

private:
    void handle_mpv_event(mpv_event *event);
    void applyMute();
    void observeProperties();

    mpv_handle *mpv;
    bool mMuted = false;
    bool mAudioOutputActive = false;
#ifndef PLAYER_MPV_USE_WID
    static void on_update(void *ctx);
    mpv_render_context *mpv_gl = nullptr;
#endif
};

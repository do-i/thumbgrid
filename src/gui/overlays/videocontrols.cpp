#include "videocontrols.h"
#include "ui_videocontrols.h"
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QWidget *createVideoControlSeparator(QWidget *parent) {
    QWidget *separator = new QWidget(parent);
    separator->setAccessibleName("VideoControlSeparator");
    separator->setFixedSize(1, 14);
    return separator;
}
}

VideoControls::VideoControls(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::VideoControls),
    loopABButton(new QPushButton("A-B", this)),
    speedButton(new QPushButton("1.00x", this)),
    volumeMuteButton(new QPushButton(tr("Mute"), this)),
    resetSpeedButton(new QPushButton(tr("Reset 1.00x"), this)),
    volumeSlider(new QSlider(Qt::Horizontal)),
    speedSlider(new QSlider(Qt::Horizontal)),
    volumePopupLabel(new QLabel("100%")),
    speedPopupLabel(new QLabel("1.00x")),
    volumePopup(new QFrame(this, Qt::Popup)),
    speedPopup(new QFrame(this, Qt::Popup)),
    loopABState(LOOP_AB_CLEAR),
    loopStartPosition(0)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_NoMousePropagation, true);
    setFixedHeight(26);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    ui->horizontalLayout->setContentsMargins(3, 1, 3, 1);
    ui->horizontalLayout->setSpacing(3);
    ui->horizontalLayout_2->setSpacing(2);
    ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    ui->horizontalSpacer_2->changeSize(4, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    volumePopup->setAccessibleName("VideoControlVolumePopup");
    speedPopup->setAccessibleName("VideoControlPopup");
    auto *volumePopupLayout = new QVBoxLayout(volumePopup);
    volumePopupLayout->setContentsMargins(8, 8, 8, 8);
    volumePopupLayout->setSpacing(6);
    auto *speedPopupLayout = new QVBoxLayout(speedPopup);
    speedPopupLayout->setContentsMargins(8, 8, 8, 8);
    speedPopupLayout->setSpacing(6);
    hide();
    ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    ui->pauseButton->setAction("pauseVideo");
    ui->pauseButton->setToolTip(tr("Play / pause"));
    ui->prevFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-backwards24.png");
    ui->prevFrameButton->setAction("frameStepBack");
    ui->prevFrameButton->setToolTip(tr("Previous frame"));
    ui->nextFrameButton->setIconPath(":res/icons/common/buttons/videocontrols/skip-forward24.png");
    ui->nextFrameButton->setAction("frameStep");
    ui->nextFrameButton->setToolTip(tr("Next frame"));
    ui->muteButton->setIconPath(":/res/icons/common/buttons/videocontrols/mute-on24.png");
    ui->muteButton->setAction("");
    ui->muteButton->setToolTip(tr("Volume"));
    ui->pauseButton->setFixedSize(24, 24);
    ui->prevFrameButton->setFixedSize(24, 24);
    ui->nextFrameButton->setFixedSize(24, 24);
    ui->muteButton->setFixedSize(24, 24);
    ui->seekBar->setAccessibleName("VideoControlSeekSlider");
    ui->seekBar->setToolTip(tr("Seek"));
    ui->seekBar->setMinimumSize(81, 24);
    ui->seekBar->setMaximumWidth(QWIDGETSIZE_MAX);
    ui->seekBar->setMaximumHeight(24);
    ui->seekBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->seekBar->setTickPosition(QSlider::TicksBelow);
    QFont timeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    timeFont.setPointSize(qMax(8, timeFont.pointSize() - 1));
    ui->positionLabel->setFont(timeFont);
    ui->durationLabel->setFont(timeFont);
    ui->positionLabel->setFixedWidth(50);
    ui->durationLabel->setFixedWidth(50);
    ui->positionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->durationLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    loopABButton->setAccessibleName("VideoControlTextButton");
    loopABButton->setFocusPolicy(Qt::NoFocus);
    loopABButton->setToolTip(tr("Loop A-B"));
    loopABButton->setFixedSize(34, 24);
    ui->horizontalLayout_2->insertWidget(2, loopABButton);

    ui->horizontalLayout->insertWidget(1, createVideoControlSeparator(this));

    volumeSlider->setAccessibleName("VideoControlVolumeSlider");
    volumeSlider->setFocusPolicy(Qt::NoFocus);
    volumeSlider->setToolTip(tr("Volume"));
    volumeSlider->setRange(0, 100);
    volumeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    volumeSlider->setMinimumWidth(180);
    volumeSlider->setTickPosition(QSlider::TicksBelow);
    volumeSlider->setTickInterval(10);
    volumeSlider->setValue(settings->volume());
    volumePopupLabel->setAlignment(Qt::AlignCenter);
    volumePopupLabel->setToolTip(tr("Volume"));
    updateVolumeText(volumeSlider->value());
    volumeMuteButton->setAccessibleName("VideoControlPopupButton");
    volumeMuteButton->setFocusPolicy(Qt::NoFocus);
    volumeMuteButton->setFixedSize(180, 24);
    volumePopupLayout->addWidget(volumePopupLabel);
    volumePopupLayout->addWidget(volumeMuteButton);
    volumePopupLayout->addWidget(volumeSlider);

    ui->horizontalLayout->insertWidget(4, createVideoControlSeparator(this));

    speedButton->setAccessibleName("VideoControlTextButton");
    speedButton->setFocusPolicy(Qt::NoFocus);
    speedButton->setToolTip(tr("Playback speed"));
    speedButton->setFixedSize(44, 24);
    ui->horizontalLayout->insertWidget(5, speedButton);

    speedSlider->setAccessibleName("VideoControlSpeedSlider");
    speedSlider->setFocusPolicy(Qt::NoFocus);
    speedSlider->setToolTip(tr("Playback speed"));
    speedSlider->setRange(25, 400);
    speedSlider->setSingleStep(25);
    speedSlider->setPageStep(25);
    speedSlider->setTickPosition(QSlider::TicksBelow);
    speedSlider->setTickInterval(25);
    speedSlider->setFixedSize(180, 28);
    speedSlider->setValue(100);
    resetSpeedButton->setAccessibleName("VideoControlPopupButton");
    resetSpeedButton->setFocusPolicy(Qt::NoFocus);
    resetSpeedButton->setFixedSize(180, 24);
    speedPopupLabel->setAlignment(Qt::AlignCenter);
    speedPopupLabel->setToolTip(tr("Playback speed"));
    speedPopupLayout->addWidget(speedPopupLabel);
    speedPopupLayout->addWidget(resetSpeedButton);
    speedPopupLayout->addWidget(speedSlider);
    ui->horizontalLayout->insertWidget(6, createVideoControlSeparator(this));

    lastPosition = -1;

    connect(ui->seekBar, &VideoSlider::sliderMovedX, this, &VideoControls::seek);
    connect(ui->muteButton, &ActionButton::clicked, this, [this]() {
        showPopupBelow(ui->muteButton, volumePopup);
    });
    connect(volumeMuteButton, &QPushButton::clicked, this, &VideoControls::toggleMuteRequested);
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        updateVolumeText(value);
        emit volumeChanged(value);
    });
    connect(speedSlider, &QSlider::valueChanged, this, [this](int value) {
        const double speed = value / 100.0;
        updateSpeedText(value);
        emit playbackSpeedChanged(speed);
    });
    connect(speedButton, &QPushButton::clicked, this, [this]() {
        showPopupBelow(speedButton, speedPopup);
    });
    connect(resetSpeedButton, &QPushButton::clicked, this, &VideoControls::resetPlaybackSpeed);
    connect(loopABButton, &QPushButton::clicked, this, [this]() {
        if(loopABState == LOOP_AB_CLEAR) {
            loopABState = LOOP_AB_START_SET;
            loopStartPosition = qMax(0, lastPosition);
            updateLoopButton();
        } else if(loopABState == LOOP_AB_START_SET) {
            const int endPosition = qMax(loopStartPosition + 1, lastPosition);
            loopABState = LOOP_AB_ACTIVE;
            emit loopABChanged(loopStartPosition, endPosition);
            updateLoopButton();
        } else {
            resetLoopAB();
            emit loopABChanged(-1, -1);
        }
    });
    updateLoopButton();
    updateResponsiveVisibility();
}

VideoControls::~VideoControls() {
    delete ui;
}

void VideoControls::setMode(PlaybackMode _mode) {
    mode = _mode;
    ui->muteButton->setVisible( (mode == PLAYBACK_VIDEO) );
    speedButton->setVisible( (mode == PLAYBACK_VIDEO) );
    loopABButton->setVisible( (mode == PLAYBACK_VIDEO) );
    if(mode != PLAYBACK_VIDEO) {
        volumePopup->hide();
        speedPopup->hide();
    }
    updateResponsiveVisibility();
}

void VideoControls::setPlaybackDuration(int duration) {
    QString durationStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = duration;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        durationStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            durationStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        durationStr = QString::number(duration);
    }
    ui->seekBar->setRange(0, duration - 1);
    // Keep a sensible, roughly constant number of seek ticks (~10, like the
    // volume slider) regardless of clip length.
    ui->seekBar->setTickInterval(qMax(1, duration / 10));
    ui->durationLabel->setText(durationStr);
    ui->positionLabel->setText(durationStr);
    updateGeometry();
    ui->positionLabel->setText("");
    resetLoopAB();
}

void VideoControls::setPlaybackPosition(int position) {
    if(position == lastPosition)
        return;
    QString positionStr;
    if(mode == PLAYBACK_VIDEO) {
        int _time = position;
        int hours = _time / 3600;
        _time -= hours * 3600;
        int minutes = _time / 60;
        int seconds = _time - minutes * 60;
        positionStr = QString("%1").arg(minutes, 2, 10, QChar('0')) + ":" +
                      QString("%1").arg(seconds, 2, 10, QChar('0'));
        if(hours)
            positionStr.prepend(QString("%1").arg(hours, 2, 10, QChar('0')) + ":");
    } else {
        positionStr = QString::number(position + 1);
    }
    ui->positionLabel->setText(positionStr);
    ui->seekBar->blockSignals(true);
    ui->seekBar->setValue(position);
    ui->seekBar->blockSignals(false);
    lastPosition = position;
}

void VideoControls::onPlaybackPaused(bool mode) {
    if(mode)
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/play24.png");
    else
        ui->pauseButton->setIconPath(":res/icons/common/buttons/videocontrols/pause24.png");
}

void VideoControls::onVideoMuted(bool mode) {
    if(mode) {
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-on24.png");
        volumeMuteButton->setText(tr("Unmute"));
    } else {
        ui->muteButton->setIconPath(":res/icons/common/buttons/videocontrols/mute-off24.png");
        volumeMuteButton->setText(tr("Mute"));
    }
}

void VideoControls::setVolume(int volume) {
    const QSignalBlocker blocker(volumeSlider);
    const int clampedVolume = qBound(0, volume, 100);
    volumeSlider->setValue(clampedVolume);
    updateVolumeText(clampedVolume);
}

void VideoControls::resetLoopAB() {
    loopABState = LOOP_AB_CLEAR;
    loopStartPosition = 0;
    updateLoopButton();
}

void VideoControls::updateLoopButton() {
    loopABButton->setProperty("checked", loopABState != LOOP_AB_CLEAR);
    if(loopABState == LOOP_AB_CLEAR)
        loopABButton->setText("A-B");
    else if(loopABState == LOOP_AB_START_SET)
        loopABButton->setText("A...");
    else
        loopABButton->setText("A-B");
    loopABButton->style()->unpolish(loopABButton);
    loopABButton->style()->polish(loopABButton);
}

void VideoControls::showPopupBelow(QWidget *anchor, QFrame *popup) {
    popup->adjustSize();
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height() + 2));
    popup->move(pos);
    popup->show();
}

void VideoControls::resetPlaybackSpeed() {
    if(speedSlider->value() == 100) {
        updateSpeedText(100);
        emit playbackSpeedChanged(1.0);
        return;
    }
    speedSlider->setValue(100);
}

void VideoControls::updateVolumeText(int volume) {
    volumePopupLabel->setText(QString::number(qBound(0, volume, 100)) + "%");
}

void VideoControls::updateSpeedText(int value) {
    const QString text = QString::number(value / 100.0, 'f', 2) + "x";
    speedButton->setText(text);
    speedPopupLabel->setText(text);
}

void VideoControls::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateResponsiveVisibility();
}

void VideoControls::updateResponsiveVisibility() {
    const bool compact = width() < 340;
    const bool veryCompact = width() < 285;
    ui->durationLabel->setVisible(!compact);
    ui->separatorLabel->setVisible(!compact);
    ui->positionLabel->setVisible(!veryCompact);
}

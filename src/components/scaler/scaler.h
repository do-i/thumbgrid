#pragma once

#include <QObject>
#include <QThreadPool>
#include <QThread>
#include <QMutex>
#include "components/cache/cache.h"
#include "scalerrequest.h"
#include "scalerrunnable.h"

class Scaler : public QObject {
    Q_OBJECT
public:
    explicit Scaler(Cache *_cache, QObject *parent = nullptr);

signals:
    void scalingFinished(QPixmap* result, const ScalerRequest& request);
    void acceptScalingResult(QImage *image, const ScalerRequest& req);
    void startBufferedRequest();

public slots:
    void requestScaled(const ScalerRequest& req);

private slots:
    void onTaskStart(const ScalerRequest& req);
    void onTaskFinish(QImage* scaled, const ScalerRequest& req);
    void slotStartBufferedRequest();
    void slotForwardScaledResult(QImage *image, ScalerRequest req);

private:
    QThreadPool *pool;
    ScalerRunnable *runnable;
    bool buffered, running;
    clock_t currentRequestTimestamp;
    ScalerRequest bufferedRequest, startedRequest;

    Cache *cache;

    void startRequest(ScalerRequest req);

    QSemaphore *sem;
};

#include "scaler.h"

#include <memory>
#include <utility>

/* What this should do in theory:
 * 1 request comes
 * 2 we run it
 * 3a if during scaling no new requests came, we return the result and forget about it. end.
 * 3b if some requests did come, by the end of current task we dispose of its result,
 *    start the last task that came and ignore the middle ones.
 */

Scaler::Scaler(Cache *_cache, QObject *parent)
    : QObject(parent),
      buffered(false),
      running(false),
      currentRequestTimestamp(0),
      cache(_cache)
{
    sem = new QSemaphore(1);
    pool = new QThreadPool(this);
    pool->setMaxThreadCount(1);
    runnable = new ScalerRunnable();
    runnable->setAutoDelete(false);
    connect(this, &Scaler::startBufferedRequest, this, &Scaler::slotStartBufferedRequest, Qt::DirectConnection);
    connect(runnable, &ScalerRunnable::started, this, &Scaler::onTaskStart, Qt::DirectConnection);
    connect(runnable, &ScalerRunnable::finished, this, &Scaler::onTaskFinish, Qt::DirectConnection);
    connect(this, &Scaler::acceptScalingResult, this, &Scaler::slotForwardScaledResult, Qt::QueuedConnection);
}

void Scaler::requestScaled(const ScalerRequest& req) {
    sem->acquire(1);
    if(!running) {
//////////////////////////////////
        if(!buffered) {
            bufferedRequest = req;
            buffered = true;
            cache->reserve(req.image->fileName());
            startRequest(req);
        } else if(bufferedRequest.image != req.image) {
            cache->reserve(req.image->fileName());
            auto tmp = bufferedRequest;
            bufferedRequest = req;
            buffered = true;
            if(startedRequest.image != tmp.image) {
                cache->release(tmp.image->fileName());
            }
        } else {
            bufferedRequest = req;
            buffered = true;
        }
//////////////////////////////
    } else {
        if(!buffered) {
            if(req.image != startedRequest.image)
                cache->reserve(req.image->fileName());
            bufferedRequest = req;
            buffered = true;
        } else {
            if(req.image == bufferedRequest.image) {
                bufferedRequest = req;
                buffered = true;
            } else {
                if(bufferedRequest.image != startedRequest.image) {
                    cache->release(bufferedRequest.image->fileName());
                }
                if(req.image != startedRequest.image)
                    cache->reserve(req.image->fileName());
                bufferedRequest = req;
                buffered = true;
            }
        }
    }
    sem->release(1);
}

void Scaler::onTaskStart(const ScalerRequest& req) {
    sem->acquire(1);
    running = true;
    // clear buffered flag if there were no requests after us
    if(buffered && bufferedRequest == req) {
        buffered = false;
    }
    startedRequest = req;
    sem->release(1);
}

void Scaler::onTaskFinish(QImage *scaled, const ScalerRequest& req) {
    // adopt the raw hand-off from ScalerRunnable::finished immediately;
    // released again only across the queued acceptScalingResult hop
    // (the tolerated exception, see CONTRIBUTING "Ownership")
    std::unique_ptr<QImage> image(scaled);
    sem->acquire(1);
    running = false;
    if(!buffered || bufferedRequest.image != req.image)
        cache->release(req.image->fileName());
    if(buffered) {
        emit startBufferedRequest();
        sem->release(1);
    } else {
        sem->release(1);
        emit acceptScalingResult(image.release(), req);
    }
}

void Scaler::slotStartBufferedRequest() {
    startRequest(bufferedRequest);
}

void Scaler::slotForwardScaledResult(QImage *image, ScalerRequest req) {
    std::unique_ptr<QImage> scaled(image);
    QPixmap *pixmap = new QPixmap(QPixmap::fromImage(*scaled));
    emit scalingFinished(pixmap, std::move(req));
}

void Scaler::startRequest(ScalerRequest req) {
    runnable->setRequest(std::move(req));
    pool->start(runnable);
}

#include "loaderrunnable.h"

#include <QElapsedTimer>
#include <utility>

LoaderRunnable::LoaderRunnable(QString _path) : path(std::move(_path)) {
}

void LoaderRunnable::run() {
    //QElapsedTimer t;
    //t.start();
    auto image = ImageFactory::createImage(path);
    //qDebug() << "L: " << t.elapsed();
    emit finished(image, path);
}

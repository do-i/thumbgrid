#include "loaderrunnable.h"

#include <QElapsedTimer>
#include <utility>

LoaderRunnable::LoaderRunnable(QString _path) : path(std::move(_path)) {
}

void LoaderRunnable::run() {
    auto image = ImageFactory::createImage(path);
    emit finished(image, path);
}

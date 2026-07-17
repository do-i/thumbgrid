#pragma once

#include <QObject>
#include <QRunnable>
#include "utils/imagefactory.h"

class LoaderRunnable: public QObject, public QRunnable
{
    Q_OBJECT
public:
    LoaderRunnable(QString _path);
    void run() override;
private:
    QString path;
signals:
    void finished(const std::shared_ptr<Image>&, const QString&);
    void failed(const QString&);
};

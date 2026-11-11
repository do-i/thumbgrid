#pragma once

#include "linuxfsevent.h"
#include "../watcherworker.h"

class LinuxWorker : public WatcherWorker
{
    Q_OBJECT
public:
    LinuxWorker();
    ~LinuxWorker() override;

    void setDescriptor(int desc);
    void handleErrorCode(int code);

    void run() override;
    void wake() override;

signals:
    void fileEvent(LinuxFsEvent* event);

private:
    int fd;
    int wakeFd;
};

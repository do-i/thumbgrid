#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QThread>
#include <QDebug>

#include "linuxworker.h"
#include "utils/logging.h"

#define TAG         "[LinuxWatcherWorker]"
constexpr int TIMEOUT = 300; // ms

LinuxWorker::LinuxWorker() :
    fd(-1),
    wakeFd(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
{
}

LinuxWorker::~LinuxWorker() {
    if(wakeFd != -1)
        close(wakeFd);
}

void LinuxWorker::wake() {
    if(wakeFd == -1)
        return;
    const uint64_t one = 1;
    if(write(wakeFd, &one, sizeof(one)) == -1)
        qCWarning(logDirManager) << TAG << strerror(errno);
}

void LinuxWorker::setDescriptor(int desc) {
    fd = desc;
}

void LinuxWorker::run() {
    emit started();

    if (fd == -1) {
        qCWarning(logDirManager) << TAG << "File descriptor isn't set! Stopping";
        emit finished();
        return;
    }

    while (isRunning) {
        int errorCode = 0;
        uint bytesAvailable = 0;

        // File descriptor event struct for polling service
        pollfd pollDescriptors[2] = { { fd, POLLIN, 0 }, { wakeFd, POLLIN, 0 } };

        // Freeze thread till next event
        errorCode = poll(pollDescriptors, wakeFd == -1 ? 1 : 2, TIMEOUT);
        handleErrorCode(errorCode);
        if(pollDescriptors[1].revents & POLLIN) {
            uint64_t counter;
            if(read(wakeFd, &counter, sizeof(counter)) == -1)
                handleErrorCode(-1);
        }
        if(!isRunning)
            break;

        // Check how many bytes available
        errorCode = ioctl(fd, FIONREAD, &bytesAvailable);
        handleErrorCode(errorCode);

        if (bytesAvailable == 0) {
            continue;
        }

        char* eventData = new char[bytesAvailable];
        errorCode = read(fd, eventData, bytesAvailable);
        handleErrorCode(errorCode);

        emit fileEvent(new LinuxFsEvent(eventData, bytesAvailable));
    }

    emit finished();
}

void LinuxWorker::handleErrorCode(int code) {
    if (code == -1) {
        qCWarning(logDirManager) << TAG << strerror(errno);
        emit error(strerror(errno));
    }
}

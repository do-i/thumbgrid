#pragma once

#include "../directorywatcher.h"

class LinuxWatcherPrivate;

class LinuxWatcher : public DirectoryWatcher {
    Q_OBJECT
public:
    explicit LinuxWatcher();
    ~LinuxWatcher() override;
    void setWatchPath(const QString& p) override;

private:
    Q_DECLARE_PRIVATE(LinuxWatcher)
};

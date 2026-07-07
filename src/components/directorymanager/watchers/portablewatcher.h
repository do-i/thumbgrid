#pragma once

#include "directorywatcher.h"

class PortableWatcherPrivate;

class PortableWatcher : public DirectoryWatcher
{
    Q_OBJECT
public:
    PortableWatcher();

    void setWatchPath(const QString &watchPath) override;
    void observe() override;
    void stopObserving() override;
    bool isObserving() override;

private:
    Q_DECLARE_PRIVATE(PortableWatcher)
};

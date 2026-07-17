#pragma once

#include <QObject>

class LinuxFsEvent : public QObject
{
public:
    LinuxFsEvent(char* data, uint dataSize);
    ~LinuxFsEvent() override;

    uint dataSize() const;
    void setDataSize(uint dataSize);

    char *data() const;
    void setData(char *data);

private:
    char* mData;
    uint mDataSize;

};

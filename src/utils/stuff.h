#pragma once

#include <QString>

#include <string>

#ifdef _WIN32
    #define StdString std::wstring
#else
    #define StdString std::string
#endif

int clamp(int x, int lower, int upper);
StdString toStdString(const QString& str);
QString fromStdString(StdString str);

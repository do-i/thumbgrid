#pragma once

// QString <-> native std::filesystem path string conversions.
// Windows filesystem APIs want wide strings; everywhere else UTF-8 works.

#include <QString>

#include <string>

#ifdef _WIN32
using StdString = std::wstring;
#else
using StdString = std::string;
#endif

inline StdString toStdString(const QString& str) {
#ifdef _WIN32
    return str.toStdWString();
#else
    return str.toStdString();
#endif
}

inline QString fromStdString(const StdString& str) {
#ifdef _WIN32
    return QString::fromStdWString(str);
#else
    return QString::fromStdString(str);
#endif
}

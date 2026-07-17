#include "stuff.h"

int clamp(int x, int lower, int upper) {
    return qMin(upper, qMax(x, lower));
}

StdString toStdString(const QString& str) {
#ifdef _WIN32
    return str.toStdWString();
#else
    return str.toStdString();
#endif
}

QString fromStdString(StdString str) {
#ifdef _WIN32
    return QString::fromStdWString(str);
#else
    return QString::fromStdString(str);
#endif
}

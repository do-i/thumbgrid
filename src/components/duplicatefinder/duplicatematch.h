#pragma once

#include <QMetaType>
#include <QSize>
#include <QString>
#include <QStringList>

struct DuplicateSearchRequest {
    enum Mode { SINGLE_IMAGE, COMPARE_FOLDERS, WITHIN_FOLDERS };
    Mode mode = SINGLE_IMAGE;
    QString sourceImage;        // SINGLE_IMAGE only
    QStringList sourceFolders;  // COMPARE_FOLDERS only
    QStringList targetFolders;  // search scope in all modes
    bool recursive = true;
    qreal similarityPercent = 87.0;
    bool matchRotated = false;
    bool matchMirrored = false;
};

struct DuplicateMatch {
    QString sourcePath;   // the reference image of the group
    QString matchPath;
    qreal similarity = 0; // percent; 100 for byte-identical
    bool exact = false;   // byte-identical content
    QSize sourceDimensions;
    QSize matchDimensions;
    qint64 sourceFileSize = 0;
    qint64 matchFileSize = 0;
};
Q_DECLARE_METATYPE(DuplicateMatch)

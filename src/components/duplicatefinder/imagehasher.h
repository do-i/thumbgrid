#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <optional>

// 64-bit DCT perceptual hash (docs/003 §2.1). Pure functions, no state.
class ImageHasher {
public:
    static constexpr int HASH_BITS = 64;

    // Perceptual hash of an already-decoded image.
    static quint64 phash(const QImage &image);
    // Decodes at reduced size (EXIF orientation applied) and hashes.
    static std::optional<quint64> phashFromFile(const QString &path);
    // Dihedral variant hashes: identity first, then the requested
    // rotations (90/180/270) and/or mirrors. Both flags -> 8 variants.
    static QList<quint64> phashVariants(const QImage &image, bool rotations, bool mirrors);

    static int hammingDistance(quint64 a, quint64 b);
    static qreal similarityPercent(quint64 a, quint64 b);
    // Max hamming distance that still satisfies a similarity threshold,
    // e.g. 87% -> 8.
    static int maxDistanceForSimilarity(qreal percent);

    // SHA-256 of file contents for exact-duplicate detection; empty on error.
    static QByteArray contentHash(const QString &path);
};

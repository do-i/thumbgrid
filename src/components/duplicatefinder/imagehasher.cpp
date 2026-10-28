#include "imagehasher.h"

#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QtAlgorithms>
#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int SAMPLE_SIZE = 32;   // DCT input
constexpr int BLOCK_SIZE = 8;     // low-frequency block
constexpr int DECODE_SIZE = 256;  // reduced decode target for large files

using Sample = std::array<float, SAMPLE_SIZE * SAMPLE_SIZE>;

// Exact area-weighted downsample to 32x32, rounded to 8 bits: what OpenCV's
// INTER_AREA produced, so hashes stay comparable across the switch.
bool toSample(const QImage &image, Sample &out) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if(gray.isNull())
        return false;
    if(gray.width() < SAMPLE_SIZE || gray.height() < SAMPLE_SIZE)
        gray = gray.scaled(SAMPLE_SIZE, SAMPLE_SIZE, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    // float weights normalized per axis, row sums then rows, as OpenCV accumulates
    const int w = gray.width(), h = gray.height();
    const double sx = double(w) / SAMPLE_SIZE, sy = double(h) / SAMPLE_SIZE;
    for(int oy = 0; oy < SAMPLE_SIZE; oy++) {
        const double y0 = oy * sy, y1 = y0 + sy;
        for(int ox = 0; ox < SAMPLE_SIZE; ox++) {
            const double x0 = ox * sx, x1 = x0 + sx;
            float sum = 0.f;
            for(int y = int(y0); y < h && y < y1; y++) {
                const float beta = float(std::min(y1, y + 1.0) - std::max(y0, double(y))) / float(sy);
                const uchar *line = gray.constScanLine(y);
                float row = 0.f;
                for(int x = int(x0); x < w && x < x1; x++)
                    row += line[x] * (float(std::min(x1, x + 1.0) - std::max(x0, double(x))) / float(sx));
                sum += beta * row;
            }
            out[oy * SAMPLE_SIZE + ox] = float(std::lrintf(sum));
        }
    }
    return true;
}

// Orthonormal 2D DCT-II; only the top-left 8x8 block is needed.
void lowFrequencyDct(const Sample &in, float *out) {
    static const auto basis = [] {
        std::array<std::array<double, SAMPLE_SIZE>, BLOCK_SIZE> b{};
        for(int k = 0; k < BLOCK_SIZE; k++)
            for(int n = 0; n < SAMPLE_SIZE; n++)
                b[k][n] = std::sqrt((k ? 2.0 : 1.0) / SAMPLE_SIZE) * std::cos(M_PI * (n + 0.5) * k / SAMPLE_SIZE);
        return b;
    }();
    double rows[BLOCK_SIZE][SAMPLE_SIZE];
    for(int k = 0; k < BLOCK_SIZE; k++)
        for(int x = 0; x < SAMPLE_SIZE; x++) {
            double s = 0;
            for(int y = 0; y < SAMPLE_SIZE; y++)
                s += basis[k][y] * in[y * SAMPLE_SIZE + x];
            rows[k][x] = s;
        }
    for(int r = 0; r < BLOCK_SIZE; r++)
        for(int k = 0; k < BLOCK_SIZE; k++) {
            double s = 0;
            for(int x = 0; x < SAMPLE_SIZE; x++)
                s += basis[k][x] * rows[r][x];
            out[r * BLOCK_SIZE + k] = float(s);
        }
}

// Each coefficient of the 8x8 block thresholded against the block's median
// (DC excluded from the median).
quint64 hashFromSample(const Sample &sample) {
    float coeffs[BLOCK_SIZE * BLOCK_SIZE];
    lowFrequencyDct(sample, coeffs);
    float sorted[BLOCK_SIZE * BLOCK_SIZE - 1];
    std::copy(coeffs + 1, coeffs + BLOCK_SIZE * BLOCK_SIZE, sorted);
    std::nth_element(sorted, sorted + 31, sorted + BLOCK_SIZE * BLOCK_SIZE - 1);
    float median = sorted[31];
    quint64 hash = 0;
    for(int i = 0; i < BLOCK_SIZE * BLOCK_SIZE; i++)
        if(coeffs[i] > median)
            hash |= 1ULL << i;
    return hash;
}

enum class Turn { Clockwise, Half, CounterClockwise };

Sample rotated(const Sample &in, Turn turn) {
    constexpr int n = SAMPLE_SIZE - 1;
    Sample out;
    for(int y = 0; y < SAMPLE_SIZE; y++)
        for(int x = 0; x < SAMPLE_SIZE; x++) {
            int sy = y, sx = x;
            switch(turn) {
            case Turn::Clockwise:        sy = n - x; sx = y;     break;
            case Turn::Half:             sy = n - y; sx = n - x; break;
            case Turn::CounterClockwise: sy = x;     sx = n - y; break;
            }
            out[y * SAMPLE_SIZE + x] = in[sy * SAMPLE_SIZE + sx];
        }
    return out;
}

Sample mirrored(const Sample &in) {
    Sample out;
    for(int y = 0; y < SAMPLE_SIZE; y++)
        for(int x = 0; x < SAMPLE_SIZE; x++)
            out[y * SAMPLE_SIZE + x] = in[y * SAMPLE_SIZE + SAMPLE_SIZE - 1 - x];
    return out;
}

} // namespace

quint64 ImageHasher::phash(const QImage &image) {
    Sample sample;
    if(!toSample(image, sample))
        return 0;
    return hashFromSample(sample);
}

std::optional<quint64> ImageHasher::phashFromFile(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if(!reader.canRead())
        return std::nullopt;
    QSize size = reader.size();
    if(size.isValid() && (size.width() > DECODE_SIZE || size.height() > DECODE_SIZE))
        reader.setScaledSize(QSize(DECODE_SIZE, DECODE_SIZE));
    QImage image = reader.read();
    if(image.isNull())
        return std::nullopt;
    return phash(image);
}

QList<quint64> ImageHasher::phashVariants(const QImage &image, bool rotations, bool mirrors) {
    QList<quint64> hashes;
    Sample sample;
    if(!toSample(image, sample))
        return hashes;
    hashes << hashFromSample(sample);
    const Turn turns[] = {Turn::Clockwise, Turn::Half, Turn::CounterClockwise};
    if(rotations)
        for(Turn turn : turns)
            hashes << hashFromSample(rotated(sample, turn));
    if(mirrors) {
        const Sample flipped = mirrored(sample);
        hashes << hashFromSample(flipped);
        if(rotations)
            for(Turn turn : turns)
                hashes << hashFromSample(rotated(flipped, turn));
    }
    return hashes;
}

int ImageHasher::hammingDistance(quint64 a, quint64 b) {
    return qPopulationCount(a ^ b);
}

qreal ImageHasher::similarityPercent(quint64 a, quint64 b) {
    return (HASH_BITS - hammingDistance(a, b)) * 100.0 / HASH_BITS;
}

int ImageHasher::maxDistanceForSimilarity(qreal percent) {
    if(percent <= 0)
        return HASH_BITS;
    if(percent >= 100)
        return 0;
    return static_cast<int>(HASH_BITS * (1.0 - percent / 100.0));
}

QByteArray ImageHasher::contentHash(const QString &path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))
        return QByteArray();
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    if(!hasher.addData(&file))
        return QByteArray();
    return hasher.result();
}

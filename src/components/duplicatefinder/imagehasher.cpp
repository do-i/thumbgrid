#include "imagehasher.h"

#include <QCryptographicHash>
#include <QFile>
#include <QImageReader>
#include <QtAlgorithms>
#include <algorithm>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int SAMPLE_SIZE = 32;   // DCT input
constexpr int BLOCK_SIZE = 8;     // low-frequency block
constexpr int DECODE_SIZE = 256;  // reduced decode target for large files

// 32x32 grayscale float matrix ready for DCT.
cv::Mat toSampleMat(const QImage &image) {
    QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    if(gray.isNull())
        return cv::Mat();
    cv::Mat src(gray.height(), gray.width(), CV_8UC1,
                const_cast<uchar *>(gray.constBits()),
                static_cast<size_t>(gray.bytesPerLine()));
    cv::Mat small;
    cv::resize(src, small, cv::Size(SAMPLE_SIZE, SAMPLE_SIZE), 0, 0, cv::INTER_AREA);
    cv::Mat floatMat;
    small.convertTo(floatMat, CV_32F);
    return floatMat;
}

// Hash of a prepared sample matrix: DCT, top-left 8x8, each coefficient
// thresholded against the median of the block (DC excluded from the median).
quint64 hashFromSample(const cv::Mat &sample) {
    cv::Mat dctMat;
    cv::dct(sample, dctMat);
    float coeffs[BLOCK_SIZE * BLOCK_SIZE];
    for(int row = 0; row < BLOCK_SIZE; row++)
        for(int col = 0; col < BLOCK_SIZE; col++)
            coeffs[row * BLOCK_SIZE + col] = dctMat.at<float>(row, col);
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

} // namespace

quint64 ImageHasher::phash(const QImage &image) {
    cv::Mat sample = toSampleMat(image);
    if(sample.empty())
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
    cv::Mat sample = toSampleMat(image);
    if(sample.empty())
        return hashes;
    hashes << hashFromSample(sample);
    cv::Mat variant;
    if(rotations) {
        for(auto code : {cv::ROTATE_90_CLOCKWISE, cv::ROTATE_180, cv::ROTATE_90_COUNTERCLOCKWISE}) {
            cv::rotate(sample, variant, code);
            hashes << hashFromSample(variant);
        }
    }
    if(mirrors) {
        cv::Mat mirrored;
        cv::flip(sample, mirrored, 1);
        hashes << hashFromSample(mirrored);
        if(rotations) {
            for(auto code : {cv::ROTATE_90_CLOCKWISE, cv::ROTATE_180, cv::ROTATE_90_COUNTERCLOCKWISE}) {
                cv::rotate(mirrored, variant, code);
                hashes << hashFromSample(variant);
            }
        }
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

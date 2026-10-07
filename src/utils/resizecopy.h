#pragma once

#include <QSet>
#include <QSize>
#include <QString>
#include "settings.h"

// How a batch of images should be resized. The grid applies one spec to every
// selected image, so the size is interpreted per image (see targetSize).
struct ResizeSpec {
    enum Mode : std::uint8_t {
        Percent,    // percent of each image's own size
        FitWithin,  // largest size inside `size` that keeps the image's aspect
        Exact,      // exactly `size`, aspect ignored
        LongEdge    // longer side becomes `longEdge` px, aspect kept
    };
    Mode mode = Percent;
    double percent = 100.0;
    QSize size;
    int longEdge = 0;
    // Skip any image that would not get smaller - a copy that is the same size
    // or an upscale is rarely what a batch resize from the grid is for.
    bool shrinkOnly = true;
    ScalingFilter filter = QI_FILTER_BILINEAR;
};

// Resize-to-copy primitives. Free of GUI and model state so the batch can run
// on a worker thread and the rules can be unit-tested directly.
namespace ResizeCopy {

enum class Outcome : std::uint8_t { Resized, Skipped, Failed };

struct Result {
    Outcome outcome = Outcome::Failed;
    QString destPath;
};

// The size `source` should become under `spec`, or an invalid QSize when the
// image should be skipped (no change, or not smaller while shrinkOnly is set).
QSize targetSize(const QSize &source, const ResizeSpec &spec);

// "<dir>/<base>_<W>x<H>.<ext>" next to `sourcePath`; "-1", "-2"... is appended
// before the extension while the name exists on disk or is in `reserved`.
QString copyPath(const QString &sourcePath, const QSize &target, const QSet<QString> &reserved = {});

// Loads `sourcePath` from disk (independently of any cached copy), scales it
// and saves a new file chosen by copyPath(). The original is never written.
// The chosen path is added to `reserved` so one batch never picks it twice.
Result resizeFile(const QString &sourcePath, const ResizeSpec &spec, QSet<QString> *reserved = nullptr);

} // namespace ResizeCopy

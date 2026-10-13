#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include "image.h"
#include "utils/imagelib.h"
#include <settings.h>
#include <QIcon>
#include <optional>
#include "utils/imagewriter.h"

class ImageStatic : public Image {
public:
    ImageStatic(QString _path);
    ImageStatic(std::unique_ptr<DocumentInfo> _info);
    ~ImageStatic() override;

    std::unique_ptr<QPixmap> getPixmap() override;
    std::shared_ptr<const QImage> getSourceImage();
    std::shared_ptr<const QImage> getImage() override;

    int height() override;
    int width() override;
    QSize size() override;

    bool setEditedImage(std::unique_ptr<const QImage> imageEditedNew);
    bool discardEditedImage();

    // save() in two halves, so the write itself can run off the GUI thread:
    // writeJob() snapshots what to write (empty if destPath cannot take this
    // image), ImageWriter::write() runs it, and onWritten() applies a
    // successful write back here.
    std::optional<ImageWriteJob> writeJob(const QString &destPath);
    void onWritten(const ImageWriteJob &job);

public slots:
    void crop(QRect newRect);
    bool save() override;
    bool save(QString destPath) override;

private:
    void load() override;
    std::shared_ptr<const QImage> image, imageEdited;
    void loadGeneric();
    void loadICO();
};

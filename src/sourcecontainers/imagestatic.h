#pragma once

#include <QImage>
#include <QImageWriter>
#include <QSemaphore>
#include <QCryptographicHash>
#include "image.h"
#include "utils/imagelib.h"
#include <settings.h>
#include <QIcon>

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

public slots:
    void crop(QRect newRect);
    bool save() override;
    bool save(QString destPath) override;

private:
    void load() override;
    std::shared_ptr<const QImage> image, imageEdited;
    void loadGeneric();
    void loadICO();
    QString generateHash(const QString& str);
};

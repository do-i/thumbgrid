#pragma once

#include "image.h"

// Lightweight container for TEXT documents. Like Video, it carries no pixel
// data - the viewer reads the file itself; this exists so text files flow
// through the model/loader/cache like any other document.
class TextFile : public Image {
public:
    TextFile(QString path);
    TextFile(std::unique_ptr<DocumentInfo> info);

    std::unique_ptr<QPixmap> getPixmap() override;
    std::shared_ptr<const QImage> getImage() override;
    int height() override;
    int width() override;
    QSize size() override;
    bool save() override;
    bool save(QString destPath) override;

protected:
    void load() override;
};

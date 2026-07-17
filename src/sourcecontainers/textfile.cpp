#include "textfile.h"

#include <utility>

TextFile::TextFile(QString path) : Image(std::move(path)) {
    TextFile::load();
}

TextFile::TextFile(std::unique_ptr<DocumentInfo> info) : Image(std::move(info)) {
    TextFile::load();
}

void TextFile::load() {
    // content is read by the text viewer on display; nothing to preload here
    mLoaded = true;
}

std::unique_ptr<QPixmap> TextFile::getPixmap() {
    return nullptr;
}

std::shared_ptr<const QImage> TextFile::getImage() {
    return nullptr;
}

int TextFile::height() {
    return 0;
}

int TextFile::width() {
    return 0;
}

QSize TextFile::size() {
    return QSize(0, 0);
}

bool TextFile::save() {
    return false;
}

bool TextFile::save(QString destPath) {
    Q_UNUSED(destPath)
    return false;
}

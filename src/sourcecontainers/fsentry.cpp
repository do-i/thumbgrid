#include "fsentry.h"

#include <utility>

FSEntry::FSEntry() {
}

FSEntry::FSEntry(const QString &path) {
    // error_code overloads: the entry may be racing with an external delete/rename
    namespace fs = std::filesystem;
    std::error_code ec;
    this->path = path;
    this->name = QString::fromStdString(fs::path(path.toStdString()).filename().generic_string());
    this->isDirectory = fs::is_directory(path.toStdString(), ec) && !ec;
    if(!isDirectory) {
        this->size = fs::file_size(path.toStdString(), ec);
        if(ec)
            this->size = 0;
        this->modifyTime = fs::last_write_time(path.toStdString(), ec);
        if(ec)
            this->modifyTime = fs::file_time_type();
    }
}

FSEntry::FSEntry( QString _path, QString _name, std::uintmax_t _size, std::filesystem::file_time_type _modifyTime, bool _isDirectory)
    : path(std::move(_path)),
      name(std::move(_name)),
      size(_size),
      modifyTime(_modifyTime),
      isDirectory(_isDirectory)
{
}
FSEntry::FSEntry( QString _path, QString _name, std::uintmax_t _size, bool _isDirectory)
    : path(std::move(_path)),
      name(std::move(_name)),
      size(_size),
      isDirectory(_isDirectory)
{
}
FSEntry::FSEntry( QString _path, QString _name, bool _isDirectory)
    : path(std::move(_path)),
      name(std::move(_name)),
      isDirectory(_isDirectory)
{
}
bool FSEntry::operator==(const QString &anotherPath) const {
    return this->path == anotherPath;
}

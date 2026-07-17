#include "script.h"

#include <utility>


Script::Script() : command(""), blocking(false) {
}

Script::Script(QString _path, bool _blocking)
    : command(std::move(_path)), blocking(_blocking)
{
}

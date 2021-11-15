
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>

using namespace phosphor::logging;

using phosphor::logging::level;
using phosphor::logging::log;

void log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    // find size, and make a cstring
    int strLen = std::snprintf(nullptr, 0, fmt, args) + 1;
    char* str = new char[strLen];

    // build fmt c string
    std::snprintf(str, strLen, fmt, args);
    log<level::ERR>(str);

    delete str;
    va_end(args);
}

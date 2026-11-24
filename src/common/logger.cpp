#include "common/logger.h"

#include <iostream>

namespace finesse {

void log(LogLevel level, std::string_view message) {
    const char* label = "INFO";
    if (level == LogLevel::Warning) {
        label = "WARN";
    } else if (level == LogLevel::Error) {
        label = "ERROR";
    }
    std::clog << '[' << label << "] " << message << '\n';
}

}  // namespace finesse

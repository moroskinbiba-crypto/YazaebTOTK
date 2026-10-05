#include "explorer.hpp"

#include <cstdio>

namespace ex {

void logMessage(const char* message) {
    FILE* file = std::fopen("sdmc:/switch/totk_explorer/log.txt", "ab");
    if (!file)
        return;

    std::fprintf(file, "[TOTK Explorer] %s\\n", message ? message : "");
    std::fclose(file);
}

} // namespace ex

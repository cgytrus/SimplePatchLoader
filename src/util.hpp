#pragma once

#include <string>

namespace util {
    uintptr_t getBaseFromName(const std::string& name);
    size_t getImageSize(uintptr_t base);
    void trimView(std::string_view& view);
}

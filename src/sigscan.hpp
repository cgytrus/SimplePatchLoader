#pragma once

#include <string_view>

namespace sigscan {
    ptrdiff_t findPattern(uintptr_t addr, size_t len, std::string_view& pattern);
}

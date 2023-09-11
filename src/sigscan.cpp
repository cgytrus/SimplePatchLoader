// now copied from https://github.com/cgytrus/gmml/blob/main/gmml/include/sigscan.h
// and changed to actually use c++ and support mac

// copied from https://github.com/learn-more/findpattern-bench/blob/fd26364c60c76d495fc92c6b75af72f91139ea54/patterns/learn_more.h
// and modified a tiny bit

#include "sigscan.hpp"

// http://www.unknowncheats.me/forum/c-and-c/77419-findpattern.html#post650040
// Original code by learn_more
// Fix based on suggestion from stevemk14ebr : http://www.unknowncheats.me/forum/1056782-post13.html

namespace {
    inline bool isMatch(uintptr_t addr, const uint8_t* pat, const char* msk) {
        for (size_t n = 0; msk[n] != 0; n++)
            if (msk[n] != '?' && reinterpret_cast<uint8_t*>(addr)[n] != pat[n])
                return false;
        return true;
    }
}

ptrdiff_t sigscan::findPattern(uintptr_t addr, size_t len, std::string_view& pattern) {
    size_t l = pattern.size();
    auto* patBase = new uint8_t[l >> 1];
    auto* mskBase = new char[l >> 1];
    auto* pat = patBase;
    auto* msk = mskBase;
    l = 0;
    while (!pattern.empty() && !pattern.starts_with(']')) {
        if (pattern.starts_with("??")) {
            *pat++ = 0;
            *msk++ = '?';
        }
        else {
            char c = pattern[0];
            *pat = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? 10 + (c - 'a') : c >= 'A' && c <= 'F' ? 10 + (c - 'A') : 0;
            *pat *= 16;
            c = pattern[1];
            *pat += c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? 10 + (c - 'a') : c >= 'A' && c <= 'F' ? 10 + (c - 'A') : 0;
            pat++;
            *msk++ = 'x';
        }
        pattern.remove_prefix(2);
        while (pattern.starts_with(' '))
            pattern.remove_prefix(1);
        l++;
    }
    if (pattern.starts_with(']'))
        pattern.remove_prefix(1);
    *msk = 0;
    for (size_t n = 0; n < (len - l); ++n)
        if (isMatch(addr + n, patBase, mskBase))
            return static_cast<ptrdiff_t>(n);
    delete[] patBase;
    delete[] mskBase;
    return 0;
}

#include "util.hpp"

#include <Geode/Utils.hpp>

#if defined(GEODE_IS_WINDOWS)
#include <Psapi.h>
#endif

uintptr_t util::getBaseFromName(const std::string& name) {
    if (name.empty() ||
        name == "GeometryDash.exe" ||
        name == "libcocos2dcpp.so")
        return geode::base::get();
#if defined(GEODE_IS_WINDOWS)
    return reinterpret_cast<uintptr_t>(GetModuleHandleA(name.c_str()));
#elif defined(GEODE_IS_ANDROID)
    return reinterpret_cast<uintptr_t>(dlopen(name.c_str(), RTLD_LAZY));
#else
    return nullptr;
#endif
}

size_t util::getImageSize(uintptr_t base) {
#if defined(GEODE_IS_WINDOWS)
    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), reinterpret_cast<HMODULE>(base), &info, sizeof(MODULEINFO)))
        return 0;
    return info.SizeOfImage;
#elif defined(GEODE_IS_MACOS)
    auto header = (const mach_header_64*)base;
    // https://stackoverflow.com/questions/28846503/getting-sizeofimage-and-entrypoint-of-dylib-module
    if (header == nullptr)
        return 0;

    size_t sz = sizeof(mach_header_64); // Size of the header
    sz += header->sizeofcmds; // Size of the load commands

    auto lc = (const load_command*)(header + 1);
    for (uint32_t i = 0; i < header->ncmds; i++) {
        if (lc->cmd == LC_SEGMENT)
            sz += ((const segment_command_64*)lc)->vmsize; // Size of segments
        lc = (const load_command*)((char*)lc + lc->cmdsize);
    }

    return sz;
#else
    return 0;
#endif
}

void util::trimView(std::string_view& view) {
    while(std::isspace(view[0]))
        view.remove_prefix(1);
}

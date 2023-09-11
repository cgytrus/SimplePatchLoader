#pragma once

#include <Geode/loader/Mod.hpp>
#include <ghc/filesystem.hpp>

namespace geode::dirs {
    ghc::filesystem::path getPatchesDir();
    ghc::filesystem::path getMHv5Dir();
    ghc::filesystem::path getMHv6Dir();
    ghc::filesystem::path getOldGDHMDir();
    ghc::filesystem::path getGDHMDir();
}

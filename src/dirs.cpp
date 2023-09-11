#include "dirs.hpp"

ghc::filesystem::path geode::dirs::getPatchesDir() {
    return geode::Mod::get()->getConfigDir() / "patches";
}

ghc::filesystem::path geode::dirs::getMHv5Dir() {
    return getPatchesDir() / "mhv5";
}

ghc::filesystem::path geode::dirs::getMHv6Dir() {
    return getPatchesDir() / "mhv6";
}

ghc::filesystem::path geode::dirs::getOldGDHMDir() {
    return getPatchesDir() / "old-gdhm";
}

ghc::filesystem::path geode::dirs::getGDHMDir() {
    return getPatchesDir() / "gdhm";
}

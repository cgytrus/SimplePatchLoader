#include "patches.hpp"

#include <Geode/Geode.hpp>

void patches::addToCategory(const std::string& category, const std::string& id, const PatchData& patch) {
    if (!patches.contains(category))
        patches[category] = {};
    if (patches[category].contains(id))
        geode::log::info("overriding patch {}", id);
    patches[category][id] = patch;
}

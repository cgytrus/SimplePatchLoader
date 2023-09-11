#pragma once

#include <Geode/loader/Hook.hpp>

#include <vector>
#include <unordered_map>

namespace patches {
    struct PatchData {
        std::string name;
        std::string description;
        std::vector<geode::Patch*> patches;
    };
    inline std::unordered_map<std::string, std::map<std::string, PatchData>> patches;

    void addToCategory(const std::string& category, const std::string& id, const PatchData& patch);
}

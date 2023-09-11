#include "mh.hpp"
#include "patches.hpp"
#include "dirs.hpp"

#include <extensions2.hpp>

#include <Geode/Geode.hpp>
using namespace geode::prelude;

namespace {
    void initMH() {
        static std::unordered_map<MegaHackExt::CheckBox*, std::pair<std::string, patches::PatchData>> mhCbs;
        for (const auto& [category, datas] : patches::patches) {
            auto* window = MegaHackExt::Window::Create(category.c_str());
            for (const auto& data : datas) {
                auto* cb = MegaHackExt::CheckBox::Create(data.second.name.c_str());
                mhCbs[cb] = data;
                cb->setCallback([](auto* cb, bool toggled) {
                    const auto& data = mhCbs[cb];

                    if (toggled)
                        for (const auto& patch : data.second.patches)
                            patch->apply();
                    else
                        for (const auto& patch : data.second.patches)
                            patch->restore();

                    bool enabled = !data.second.patches.empty();
                    for (const auto& patch : data.second.patches) {
                        if (patch->isApplied())
                            continue;
                        enabled = false;
                        break;
                    }

                    Mod::get()->setSavedValue<bool>(fmt::format("patch.{}", data.first), enabled);

                    if (enabled != toggled) {
                        cb->set(enabled, false);
                    }
                });
                window->add(cb);
            }
            MegaHackExt::Client::commit(window);
        }

        auto* mainWin = MegaHackExt::Window::Create("Simple Patch Loader");
        auto* butt = MegaHackExt::Button::Create("Open Patches Folder");
        butt->setCallback([](auto* butt) {
            utils::file::openFolder(dirs::getPatchesDir());
        });
        mainWin->add(butt);
        MegaHackExt::Client::commit(mainWin);
    }
}

namespace mh {
    bool tryInit() {
        auto mod = Loader::get()->getInstalledMod("absolllute.megahack");
        if (Loader::get()->isModLoaded("absolllute.megahack")) {
            initMH();
            return true;
        }
        // this assumes mh doesnt fail to load but,,,,,, whatever
        else if (mod && Loader::get()->getInstalledMod("geode.loader")->
            getSavedValue("should-load-absolllute.megahack", true)) {
            (new geode::EventListener(ModStateFilter(mod, geode::ModEventType::Loaded)))->
                bind([](ModStateEvent* e) { initMH(); });
            return true;
        }
        return false;
    }
}

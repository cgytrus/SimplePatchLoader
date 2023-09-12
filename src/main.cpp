#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/loader/SettingEvent.hpp>

#define STB_RECT_PACK_IMPLEMENTATION
#include <imgui-cocos.hpp>
#include <imstb_rectpack.h>
#include <hjfod.custom-keybinds/include/Keybinds.hpp>

#include <unordered_map>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "sigscan.hpp"
#include "dirs.hpp"
#include "mh.hpp"
#include "util.hpp"
#include "pp.hpp"
#include "patches.hpp"

using namespace geode::prelude;

Result<ptrdiff_t> parseAndDoSigScan(uintptr_t base, std::string_view& view) {
    size_t length = util::getImageSize(base);
    if (length == 0)
        return Err("invalid base address");
    auto res = sigscan::findPattern(base, length, view);
    if (res == 0)
        return Err("not found");
    return Ok(res);
}
Result<std::pair<uintptr_t, ptrdiff_t>> findAddressBySignature(const std::string& sig) {
    uintptr_t base = base::get();

    std::string_view view = sig;
    std::stack<ptrdiff_t> stack;

    // if it's neither a signature or a signature, it's a module name
    util::trimView(view);
    if (!view.starts_with("0x") && !view.starts_with('[')) {
        auto count = view.find(' ');
        if (count == std::string_view::npos)
            return Err("invalid module name");
        base = util::getBaseFromName(std::string(view.data(), count));
        view.remove_prefix(count);
    }

    util::trimView(view);
    if (view.starts_with('[')) {
        view.remove_prefix(1);
        auto scan = parseAndDoSigScan(base, view);
        if (!scan)
            return Err("sigscan failed: {}", scan.error());
        stack.push(scan.value());
    }

    util::trimView(view);
    while (!view.empty()) {
        if (view.starts_with("0x")) {
            view.remove_prefix(2);
            ptrdiff_t num = 0;
            while (!view.empty() &&
                (view[0] >= '0' && view[0] <= '9' ||
                view[0] >= 'a' && view[0] <= 'f' ||
                view[0] >= 'A' && view[0] <= 'F')) {
                char c = view[0];
                num *= 16;
                if (c >= '0' && c <= '9')
                    num += c - '0';
                else if (c >= 'a' && c <= 'f')
                    num += 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F')
                    num += 10 + (c - 'A');
                view.remove_prefix(1);
            }
            stack.push(num);
        }
        else if(view.starts_with('+')) {
            view.remove_prefix(1);
            auto b = stack.top();
            stack.pop();
            auto a = stack.top();
            stack.pop();
            stack.push(a + b);
        }
        else if(view.starts_with('-')) {
            view.remove_prefix(1);
            auto b = stack.top();
            stack.pop();
            auto a = stack.top();
            stack.pop();
            stack.push(a - b);
        }
        else if(view.starts_with('*')) {
            view.remove_prefix(1);
            auto a = stack.top();
            stack.pop();
            stack.push(*reinterpret_cast<ptrdiff_t*>(base + a));
        }
        else {
            return Err("invalid operator");
        }
        util::trimView(view);
    }

    if (stack.size() != 1)
        return Err("stack should only have 1 number by the end");

    return Ok(std::pair{base, stack.top()});
}

Result<Patch*> patchModMenuOpcode(json::Value opcode, bool isReference, bool enabled) {
    if (!opcode.is_object() ||
        opcode.contains("addr") && !opcode["addr"].is_string() ||
        opcode.contains("address") && !opcode["address"].is_string() ||
        opcode.contains("on") && !opcode["on"].is_string() ||
        !opcode.contains("addr") && !opcode.contains("address")) {
        return Err("invalid opcode");
    }
    if (!opcode.contains("on"))
        return Ok(nullptr);
    std::string addrKey = opcode.contains("addr") ? "addr" : "address";

    uintptr_t addrBase = opcode.contains("lib") && opcode["lib"].is_string() ?
        util::getBaseFromName(opcode["lib"].as_string()) : base::get();

    uintptr_t addr = addrBase + std::stoul(opcode[addrKey].as_string(), nullptr, 16);
    ByteVector bytes;
    std::string on = opcode["on"].as_string();
    if (isReference) {
        auto ref = addrBase + std::stoul(on, nullptr, 16);
        bytes.push_back((ref >> 0x00) & 0xFF);
        bytes.push_back((ref >> 0x08) & 0xFF);
        bytes.push_back((ref >> 0x10) & 0xFF);
        bytes.push_back((ref >> 0x18) & 0xFF);
    }
    else {
        for (const auto& b : utils::string::split(on, " "))
            bytes.push_back(std::stoul(b, nullptr, 16));
    }

    auto patch = Mod::get()->patch(reinterpret_cast<void*>(addr), bytes);
    if (!patch)
        return Err("failed to patch: {}", patch.error());

    patch.value()->setAutoEnable(enabled);
    if (!enabled && patch.value()->isApplied())
        patch.value()->restore();

    return Ok(patch.value());
}
Result<> loadModMenu(const std::string& id, const std::string& category, json::Value json) {
    if (json.contains("hacks") && !json["hacks"].is_array() ||
        json.contains("TMGDMOD") && !json["TMGDMOD"].is_array() ||
        json.contains("mods") && !json["mods"].is_array() ||
        !json.contains("hacks") && !json.contains("TMGDMOD") && !json.contains("mods")) {
        return Err("invalid MHv5/MHv6/GDHM json");
    }
    auto hacks = (json.contains("hacks") ? json["hacks"] :
        json.contains("TMGDMOD") ? json["TMGDMOD"] :
            json["mods"]).as_array();

    std::string categoryId = id;
    if (json.contains("data") && json["data"].is_object() &&
        json["data"].contains("name") && json["data"]["name"].is_string())
        categoryId = json["data"]["name"].as_string();

    for (const auto& hack : hacks) {
        if (!hack.is_object() ||
            !hack.contains("name") || !hack["name"].is_string() ||
            hack.contains("opcodes") && !hack["opcodes"].is_array() ||
            hack.contains("references") && !hack["references"].is_array()) {
            log::warn("Invalid MHv5/MHv6/GDHM hack");
            continue;
        }

        std::string hackId = fmt::format("{}.{}", categoryId, hack["name"].as_string());

        if (!hack.contains("opcodes") && !hack.contains("references")) {
            log::info("Skipping hack {} because it's empty", hackId);
            continue;
        }

        bool enabled = Mod::get()->getSavedValue<bool>(fmt::format("patch.{}", hackId), false);

        bool skip = false;
        std::vector<Patch*> patches2;

        if (hack.contains("opcodes")) {
            auto opcodes = hack["opcodes"].as_array();
            for (const auto& opcode : opcodes) {
                auto patch = patchModMenuOpcode(opcode, false, enabled);
                if (!patch) {
                    log::warn("Invalid hack opcode: {}: {}", hack["name"].as_string(),
                        patch.error());
                    skip = true;
                    break;
                }
                if (patch.value())
                    patches2.push_back(patch.value());
            }
            if (skip)
                continue;
        }

        if (hack.contains("references")) {
            auto references = hack["references"].as_array();
            for (const auto& reference : references) {
                auto patch = patchModMenuOpcode(reference, true, enabled);
                if (!patch) {
                    log::warn("Invalid hack reference: {}: {}", hack["name"].as_string(),
                        patch.error());
                    skip = true;
                    break;
                }
                if (patch.value())
                    patches2.push_back(patch.value());
            }
            if (skip)
                continue;
        }

        patches::addToCategory(category, hackId, {
            hack["name"].as_string(),
            hack.contains("desc") ? hack["desc"].as_string() :
                hack.contains("description") ? hack["description"].as_string() : "",
            patches2
        });
    }

    return Ok();
}
void loadModMenuDir(const ghc::filesystem::path& dir, const std::string_view& title) {
    (void)file::createDirectoryAll(dir);
    for (auto& file : ghc::filesystem::directory_iterator(dir)) {
        if (file.is_directory())
            continue;
        auto json = file::readJson(file);
        if (!json) {
            log::warn("Failed to read {}: {}", file.path(), json.error());
            continue;
        }
        auto categoryId = file.path().filename().replace_extension().string();
        auto category = categoryId;
        category[0] = (char)std::toupper(category[0]);
        auto res = loadModMenu(fmt::format("{}-{}", dir.filename().string(), categoryId),
            fmt::format("{}: {}", title, category), json.value());
        if (!res)
            log::warn("Failed to load {}: {}", file.path(), res.error());
    }
}

Result<Patch*> patchListBytes(uintptr_t base, ptrdiff_t offset, const std::string& bytesStr, bool enabled) {
    ByteVector bytes;
    for (const auto& b : utils::string::split(bytesStr, " ")) {
        if (b.starts_with("ref:")) {
            auto ref = base + std::stoul(b.substr(4), nullptr, 16);
            bytes.push_back((ref >> 0x00) & 0xFF);
            bytes.push_back((ref >> 0x08) & 0xFF);
            bytes.push_back((ref >> 0x10) & 0xFF);
            bytes.push_back((ref >> 0x18) & 0xFF);
            continue;
        }
        bytes.push_back(std::stoul(b, nullptr, 16));
    }

    auto patch = Mod::get()->patch(reinterpret_cast<void*>(base + offset), bytes);
    if (!patch)
        return Err("failed to patch: {}", patch.error());

    patch.value()->setAutoEnable(enabled);
    if (!enabled && patch.value()->isApplied())
        patch.value()->restore();

    return Ok(patch.value());
}
Result<> loadList(const std::string_view& categoryId, json::Value json) {
    if (!json.contains("name") || !json["name"].is_string() ||
        !json.contains("patches") || !json["patches"].is_object())
        return Err("missing name or patches");
    std::string category = json["name"].as_string();
    for (const auto& [patchId, data] : json["patches"].as_object()) {
        if (!data.is_object() || !data.contains("name") || !data["name"].is_string() ||
            !data.contains("address") || !data["address"].is_object()) {
            log::warn("Invalid patch {}: not object, missing name or address", patchId);
            continue;
        }

        std::string id = fmt::format("{}.{}", categoryId, patchId);
        bool enabled = Mod::get()->getSavedValue<bool>(fmt::format("patch.{}", id), false);

        bool skip = false;
        std::vector<Patch*> patches2;

        for (const auto& [sig, bytesStr] : data["address"].as_object()) {
            if (!bytesStr.is_string()) {
                log::warn("Invalid patch {}: invalid bytes", patchId);
                skip = true;
                break;
            }

            auto addr = findAddressBySignature(sig);
            if (!addr) {
                log::warn("Invalid patch {}: {}", patchId, addr.error());
                skip = true;
                break;
            }
            uintptr_t base = addr.value().first;
            ptrdiff_t offset = addr.value().second;

            auto patch = patchListBytes(base, offset, bytesStr.as_string(), enabled);
            if (!patch) {
                log::warn("Invalid patch {}: {}", patchId, patch.error());
                skip = true;
                break;
            }

            patches2.push_back(patch.value());
        }
        if (skip)
            continue;

        patches::addToCategory(category, id, {
            data["name"].as_string(),
            data.contains("description") && data["description"].is_string() ?
                data["description"].as_string() : "",
            patches2
        });
    }

    return Ok();
}
void loadListDir(const ghc::filesystem::path& dir) {
    (void)file::createDirectoryAll(dir);
    for (auto& file : ghc::filesystem::directory_iterator(dir)) {
        if (file.is_directory())
            continue;
        auto json = file::readJson(file);
        if (!json) {
            log::warn("Failed to read {}: {}", file.path(), json.error());
            continue;
        }
        auto res = loadList(file.path().filename().replace_extension().string(), json.value());
        if (!res)
            log::warn("Failed to load {}: {}", file.path(), res.error());
    }
}

constexpr int sortPadding = 3;
uint8_t shouldSortWindows = 0;
std::vector<stbrp_rect> sortRects;

void loadAll() {
    loadModMenuDir(dirs::getMHv5Dir(), "MHv5");
    loadModMenuDir(dirs::getMHv6Dir(), "MHv6");
    loadModMenuDir(dirs::getOldGDHMDir(), "Old GDHM");
    loadModMenuDir(dirs::getGDHMDir(), "GDHM");
    loadListDir(dirs::getPatchesDir());
    shouldSortWindows = 3;
}

bool isOpen = false;
bool pp::blurFast = Mod::get()->getSettingValue<bool>("blur-fast");
float pp::blurAmount = (float)Mod::get()->getSettingValue<double>("blur-amount");
float pp::blurTime = (float)Mod::get()->getSettingValue<double>("blur-time");

float pp::blurTimer = 0.f;

float mhv5Download[] = { -1.f, -1.f, -1.f, -1.f };
void downloadMHv5(float* progress, const std::string& name) {
    *progress = 0.f;
    web::AsyncWebRequest()
        .progress([progress](web::SentAsyncWebRequest& sent, double current, double total) {
            *progress = total == 0.0 ? 0.f : static_cast<float>(current / total);
        })
        .fetch("https://raw.githubusercontent.com/absoIute/Mega-Hack-v5/8ff6210623d969ed24c9550104313dcd52d7b324/bin/hacks/" + name)
        .into(dirs::getMHv5Dir() / name)
        .then([progress](std::monostate) {
            *progress = -1.f;
            for (float d : mhv5Download)
                if (d >= 0.f)
                    return;
            loadModMenuDir(dirs::getMHv5Dir(), "MHv5");
            shouldSortWindows = 3;
        })
        .expect([progress](std::string const& error) {
            *progress = -1.f;
        });
}

// copied directly from imgui with a single change 👍
namespace ImGui {
    bool CoolCheckbox(const char* label, bool* v) {
        ImGuiWindow* window = GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = CalcTextSize(label, nullptr, true);

        const float square_sz = GetFrameHeight();
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect total_bb(pos, pos + ImVec2(
            square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
            label_size.y + style.FramePadding.y * 2.0f));
        ItemSize(total_bb, style.FramePadding.y);
        if (!ItemAdd(total_bb, id)) {
            IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
                g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                    (*v ? ImGuiItemStatusFlags_Checked : 0));
            return false;
        }

        bool hovered, held;
        bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
        if (pressed) {
            *v = !(*v);
            MarkItemEdited(id);
        }

        // the changes are here they make the width constant
        // and make the checkbox rect change the color instead of drawing a checkmark
        auto frame_col = GetColorU32(*v ?
            ((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_CheckMark) :
            ((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
        const ImRect check_bb(pos, pos + ImVec2(4.f, square_sz));
        ImRect use_check_bb = check_bb;
        if (!*v) {
            float pad = hovered ? square_sz * 0.3f : square_sz - 4.f;
            use_check_bb.Min += {0.f, pad * 0.5f};
            use_check_bb.Max -= {0.f, pad * 0.5f};
        }
        RenderNavHighlight(total_bb, id);
        RenderFrame(use_check_bb.Min, use_check_bb.Max, frame_col, true, style.FrameRounding);

        ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x,
            check_bb.Min.y + style.FramePadding.y);
        if (g.LogEnabled)
            LogRenderedText(&label_pos, *v ? "[x]" : "[ ]");
        if (label_size.x > 0.0f)
            RenderText(label_pos, label);

        IMGUI_TEST_ENGINE_ITEM_INFO(id, label,
            g.LastItemData.StatusFlags | ImGuiItemStatusFlags_Checkable |
                (*v ? ImGuiItemStatusFlags_Checked : 0));
        return pressed;
    }
}

void setupImGuiStyle() {
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {6.f, 6.f};
    style.WindowRounding = 3.f;
    style.FramePadding = {3.f, 3.f};
    style.FrameRounding = 2.f;
    style.ItemSpacing = {3.f, 3.f};
    style.ItemInnerSpacing = {5.f, 5.f};
    style.ScrollbarSize = 8.f;
    style.ScrollbarRounding = 16.f;
}

void windowPreBegin(size_t& sortIndex) {
    if (shouldSortWindows == 1) {
        auto rect = sortRects[sortIndex++];
        ImGui::SetNextWindowPos({
            (float)rect.x + sortPadding,
            (float)rect.y + sortPadding
        });
    }
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (pp::blurTimer == 1.f)
        ImGui::SetNextWindowSize({-1.f, -1.f});
}
void windowPreEnd() {
    ImVec2 size = ImGui::GetWindowSize();
    if (pp::blurTimer < 1.f) {
        size = ImGui::CalcWindowNextAutoFitSize(ImGui::GetCurrentWindow());
        ImGui::SetWindowSize({size.x, size.y * pp::blurTimer});
    }
    if (shouldSortWindows == 2) {
        sortRects.push_back({
            0,
            (int)size.x + sortPadding,
            (int)size.y + sortPadding
        });
    }
}

$execute {
    loadAll();

    listenForSettingChanges("blur-fast", +[](bool value) {
        pp::blurFast = value;
    });
    listenForSettingChanges("blur-amount", +[](double value) {
        pp::blurAmount = (float)value;
    });
    listenForSettingChanges("blur-time", +[](double value) {
        pp::blurTime = (float)value;
    });

    if (mh::tryInit())
        return;

    Loader::get()->queueInMainThread([]() {
        ImGuiCocos::get().setup([] {
            setupImGuiStyle();
        }).draw([] {
            if (isOpen)
                pp::blurTimer += CCDirector::get()->getDeltaTime() / pp::blurTime;
            else
                pp::blurTimer -= CCDirector::get()->getDeltaTime() / pp::blurTime;
            if (pp::blurTimer < 0.f) pp::blurTimer = 0.f;
            if (pp::blurTimer > 1.f) pp::blurTimer = 1.f;

            if (pp::blurTimer == 0.f)
                return;

            size_t sortIndex = 0;
            for (const auto& [category, datas] : patches::patches) {
                windowPreBegin(sortIndex);
                ImGui::Begin(category.c_str());
                for (const auto& [id, data] : datas) {
                    bool enabled = !data.patches.empty();
                    for (const auto& patch : data.patches) {
                        if (patch->isApplied())
                            continue;
                        enabled = false;
                        break;
                    }
                    bool changed = ImGui::CoolCheckbox(data.name.c_str(), &enabled);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", data.description.c_str());
                    if (!changed)
                        continue;
                    if (enabled)
                        for (const auto& patch : data.patches)
                            patch->apply();
                    else
                        for (const auto& patch : data.patches)
                            patch->restore();
                    Mod::get()->setSavedValue<bool>(fmt::format("patch.{}", id), enabled);
                }
                windowPreEnd();
                ImGui::End();
            }

            windowPreBegin(sortIndex);
            ImGui::Begin("Simple Patch Loader");

            if (ImGui::Button("Open Patches Folder"))
                utils::file::openFolder(dirs::getPatchesDir());

            if (ImGui::Button("Sort Windows"))
                shouldSortWindows = 3;

            if (ImGui::Button("Reload Patches"))
                loadAll();

#if defined(GEODE_IS_WINDOWS)
            bool mhv5DownloadDisabled = false;
            for (float d : mhv5Download) {
                if (d >= 0.f) {
                    mhv5DownloadDisabled = true;
                    break;
                }
            }
            ImGui::BeginDisabled(mhv5DownloadDisabled);
            if (ImGui::Button("Download Mega Hack v5 hacks")) {
                downloadMHv5(&mhv5Download[0], "bypass.json");
                downloadMHv5(&mhv5Download[1], "creator.json");
                downloadMHv5(&mhv5Download[2], "global.json");
                downloadMHv5(&mhv5Download[3], "player.json");
            }
            for (float d : mhv5Download)
                if (d >= 0.f)
                    ImGui::ProgressBar(mhv5Download[0], ImVec2(-1.f, 2.f));
            ImGui::EndDisabled();
#endif

            ImGui::Separator();

            ImGui::LabelText("", "Blur");
            if (ImGui::CoolCheckbox("Fast", &pp::blurFast))
                Mod::get()->setSettingValue<bool>("blur-fast", pp::blurFast);
            if (ImGui::SliderFloat("Amount", &pp::blurAmount, 0.f, 1.f))
                Mod::get()->setSettingValue<double>("blur-amount", pp::blurAmount);
            if (ImGui::SliderFloat("Time", &pp::blurTime, 0.f, 5.f))
                Mod::get()->setSettingValue<double>("blur-time", pp::blurTime);

            windowPreEnd();
            ImGui::End();

            if (shouldSortWindows == 2) {
                ImGuiIO& io = ImGui::GetIO();
                stbrp_context ctx;
                auto nodeCount = static_cast<int>(io.DisplaySize.x * 2.f);
                auto* nodes = new stbrp_node[nodeCount];
                memset(nodes, 0, sizeof(stbrp_node) * nodeCount);

                stbrp_init_target(&ctx, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y), nodes, nodeCount);
                stbrp_pack_rects(&ctx, sortRects.data(), static_cast<int>(sortRects.size()));

                delete[] nodes;
            }
            else if(shouldSortWindows == 1) {
                sortRects.clear();
            }

            if (shouldSortWindows > 0)
                shouldSortWindows--;

            ImGui::Render();
            auto* drawData = ImGui::GetDrawData();
            for (int i = 0; i < drawData->CmdListsCount; ++i) {
                auto* list = drawData->CmdLists[i];
                for (auto& vert : list->VtxBuffer) {
                    auto col = ImColor(vert.col);
                    col.Value.w *= pp::blurTimer;
                    vert.col = col;
                }
            }
        });
    });

    using namespace keybinds;
    BindManager::get()->registerBindable({
        "open"_spr,
        "Open",
        "",
        {Keybind::create(KEY_Tab, Modifier::None)},
        "Global"
    });
    new EventListener([=](InvokeBindEvent* event) {
        if (event->isDown())
            isOpen = !isOpen;
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "open"_spr));
}

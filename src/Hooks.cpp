#include "Hooks.h"
#include "Config.h"
#include "Classifiers.h"
#include <MinHook.h>
#include <REL/Relocation.h>
#include <REL/Offset.h>

namespace StackingPlugin {

    using HasOnlyIgnorableExtraData_t = bool(*)(RE::ExtraDataList*, char);
    using IsNotEqual_t = bool(*)(RE::ExtraDataList*, RE::ExtraDataList*, char);
    HasOnlyIgnorableExtraData_t g_origHasOnlyIgnorable = nullptr;
    IsNotEqual_t g_origIsNotEqual = nullptr;

    RE::TESBoundObject* FindOwnerObject(RE::ExtraDataList* a_target) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return nullptr;
        auto* changes = player->GetInventoryChanges();
        if (!changes || !changes->entryList) return nullptr;
        for (auto& entry : *changes->entryList) {
            if (!entry || !entry->extraLists) continue;
            for (auto& xList : *entry->extraLists) {
                if (xList == a_target) return entry->object;
            }
        }
        return nullptr;
    }

    bool ShouldSkipStolenUnstack(RE::TESBoundObject* a_obj) {
        if (!a_obj) return false;
        auto& cfg = Config::Get();
        if (!cfg.unstackStolenIncludeIngredients && a_obj->GetFormType() == RE::FormType::Ingredient)
            return true;
        return false;
    }

    bool HasOnlyIgnorableExtraData_Hook(RE::ExtraDataList* a_list, char a_checkOwnership) {
        bool result = g_origHasOnlyIgnorable(a_list, a_checkOwnership);
        if (!result)
            return false;

        auto& cfg = Config::Get();

        if (cfg.unstackStolen && a_checkOwnership && IsStolenExtraDataList(a_list)) {
            if (ShouldSkipStolenUnstack(FindOwnerObject(a_list)))
                return true;
            return false;
        }

        if (cfg.unstackQuest && IsQuestExtraDataList(a_list))
            return false;

        if (cfg.unstackFavorites && IsFavoritedExtraDataList(a_list))
            return false;

        if (cfg.unstackEquipped && a_checkOwnership && IsEquippedExtraDataList(a_list))
            return false;

        return true;
    }

    bool IsNotEqual_Hook(RE::ExtraDataList* a_lhs, RE::ExtraDataList* a_rhs, char a_param3) {
        auto& cfg = Config::Get();

        bool lhsDistinct = a_lhs && (a_lhs->HasType(RE::ExtraDataType::kTextDisplayData) ||
                                      a_lhs->HasType(RE::ExtraDataType::kEnchantment));
        bool rhsDistinct = a_rhs && (a_rhs->HasType(RE::ExtraDataType::kTextDisplayData) ||
                                      a_rhs->HasType(RE::ExtraDataType::kEnchantment));
        if (lhsDistinct || rhsDistinct)
            return g_origIsNotEqual(a_lhs, a_rhs, a_param3);

        if (cfg.unstackStolen && (IsStolenExtraDataList(a_lhs) != IsStolenExtraDataList(a_rhs)))
            return true;
        if (cfg.unstackQuest && (IsQuestExtraDataList(a_lhs) != IsQuestExtraDataList(a_rhs)))
            return true;
        if (cfg.unstackFavorites && (IsFavoritedExtraDataList(a_lhs) != IsFavoritedExtraDataList(a_rhs)))
            return true;
        if (cfg.unstackEquipped && (IsEquippedExtraDataList(a_lhs) != IsEquippedExtraDataList(a_rhs)))
            return true;

        return g_origIsNotEqual(a_lhs, a_rhs, a_param3);
    }

    void Hooks::Install() {
        Config::Get().Load();

        auto& cfg = Config::Get();
        if (cfg.debugLogging)
            SKSE::log::info("Debug logging enabled");

        SKSE::log::info("Unstacking config: stolen={} (includeIngredients={}), quest={}, favorites={}, equipped={}",
            cfg.unstackStolen, cfg.unstackStolenIncludeIngredients,
            cfg.unstackQuest, cfg.unstackFavorites, cfg.unstackEquipped);

        if (!cfg.unstackStolen && !cfg.unstackQuest && !cfg.unstackFavorites && !cfg.unstackEquipped) {
            SKSE::log::info("All unstacking disabled, skipping hooks");
            return;
        }

        if (MH_Initialize() != MH_OK) {
            SKSE::log::error("Failed to initialize MinHook");
            return;
        }

        auto resolveAddr = [](std::uint64_t a_seID, std::uint64_t a_aeID, std::uint64_t a_vrOffset) -> std::uintptr_t {
            if (REL::Module::IsVR())
                return REL::Offset(a_vrOffset).address();
            return REL::RelocationID(a_seID, a_aeID).address();
        };

        auto installHook = [](std::uintptr_t a_target, void* a_detour, void** a_original, const char* a_name) -> bool {
            auto status = MH_CreateHook(reinterpret_cast<void*>(a_target), a_detour, a_original);
            if (status != MH_OK) {
                SKSE::log::error("Failed to hook {}: {}", a_name, MH_StatusToString(status));
                return false;
            }
            MH_EnableHook(reinterpret_cast<void*>(a_target));
            return true;
        };

        if (!installHook(resolveAddr(11452, 11598, 0x11D220),
                reinterpret_cast<void*>(&HasOnlyIgnorableExtraData_Hook),
                reinterpret_cast<void**>(&g_origHasOnlyIgnorable),
                "HasOnlyIgnorableExtraData"))
            return;

        if (!installHook(resolveAddr(11448, 11594, 0x119B10),
                reinterpret_cast<void*>(&IsNotEqual_Hook),
                reinterpret_cast<void**>(&g_origIsNotEqual),
                "IsNotEqual"))
            return;

        SKSE::log::info("Hooks installed");
    }

    void Hooks::MergeInventoryLists() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;

        auto* changes = player->GetInventoryChanges();
        if (!changes || !changes->entryList) return;

        if (!g_origIsNotEqual) {
            SKSE::log::warn("MergeInventoryLists: original IsNotEqual not available, skipping");
            return;
        }

        auto& cfg = Config::Get();
        int totalMerged = 0;

        for (auto& entry : *changes->entryList) {
            if (!entry || !entry->extraLists) continue;

            std::vector<RE::ExtraDataList*> lists;
            for (auto& xList : *entry->extraLists) {
                if (xList) lists.push_back(xList);
            }

            if (lists.size() < 2) continue;

            std::vector<bool> removed(lists.size(), false);
            int mergedThisEntry = 0;

            for (std::size_t i = 0; i < lists.size(); i++) {
                if (removed[i]) continue;
                for (std::size_t j = i + 1; j < lists.size(); j++) {
                    if (removed[j]) continue;
                    if (lists[i]->HasType(RE::ExtraDataType::kTextDisplayData) ||
                        lists[j]->HasType(RE::ExtraDataType::kTextDisplayData) ||
                        lists[i]->HasType(RE::ExtraDataType::kEnchantment) ||
                        lists[j]->HasType(RE::ExtraDataType::kEnchantment))
                        continue;
                    bool eitherHotkeyed = lists[i]->HasType(RE::ExtraDataType::kHotkey) !=
                                          lists[j]->HasType(RE::ExtraDataType::kHotkey);
                    if (eitherHotkeyed) continue;
                    if (!g_origIsNotEqual(lists[i], lists[j], 1) &&
                        !g_origIsNotEqual(lists[j], lists[i], 1)) {
                        auto countI = lists[i]->GetCount();
                        auto countJ = lists[j]->GetCount();
                        lists[i]->SetCount(static_cast<std::uint16_t>(countI + countJ));
                        removed[j] = true;
                        mergedThisEntry++;
                    }
                }
            }

            if (mergedThisEntry == 0) continue;

            std::vector<RE::ExtraDataList*> surviving;
            for (std::size_t i = 0; i < lists.size(); i++) {
                if (!removed[i]) surviving.push_back(lists[i]);
            }

            entry->extraLists->clear();
            for (auto it = surviving.rbegin(); it != surviving.rend(); ++it) {
                entry->extraLists->push_front(*it);
            }

            totalMerged += mergedThisEntry;

            if (cfg.debugLogging) {
                const char* name = entry->object ? entry->object->GetName() : "???";
                SKSE::log::info("Merged {} ExtraDataLists for {} ({} remaining)",
                    mergedThisEntry, name, surviving.size());
            }
        }

        if (totalMerged > 0) {
            SKSE::log::info("Post-load merge: consolidated {} ExtraDataLists", totalMerged);
        }
    }

}

#include "Hooks.h"
#include "Config.h"
#include "Classifiers.h"
#include <MinHook.h>
#include <REL/Relocation.h>
#include <REL/Offset.h>

namespace StackingPlugin {

    using HasOnlyIgnorableExtraData_t = bool(*)(RE::ExtraDataList*, char);
    using IsNotEqual_t = bool(*)(RE::ExtraDataList*, RE::ExtraDataList*, char);
    using AddExtraList_t = void(*)(RE::InventoryEntryData*, RE::ExtraDataList*, char);

    HasOnlyIgnorableExtraData_t g_origHasOnlyIgnorable = nullptr;
    IsNotEqual_t g_origIsNotEqual = nullptr;
    AddExtraList_t g_origAddExtraList = nullptr;

    bool HasOnlyIgnorableExtraData_Hook(RE::ExtraDataList* a_list, char a_checkOwnership) {
        bool result = g_origHasOnlyIgnorable(a_list, a_checkOwnership);
        if (!result)
            return false;

        auto& cfg = Config::Get();

        if (cfg.unstackStolen && a_checkOwnership && IsStolenExtraDataList(a_list))
            return false;

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

        if (cfg.unstackStolen && IsStolenExtraDataList(a_lhs) && IsStolenExtraDataList(a_rhs))
            return false;

        if (cfg.unstackQuest && IsQuestExtraDataList(a_lhs) && IsQuestExtraDataList(a_rhs))
            return false;

        if (cfg.unstackFavorites && IsFavoritedExtraDataList(a_lhs) && IsFavoritedExtraDataList(a_rhs))
            return false;

        if (cfg.unstackEquipped && IsEquippedExtraDataList(a_lhs) && IsEquippedExtraDataList(a_rhs))
            return false;

        return g_origIsNotEqual(a_lhs, a_rhs, a_param3);
    }

    void AddExtraList_Hook(RE::InventoryEntryData* a_this, RE::ExtraDataList* a_extra, char a_merge) {
        if (!a_merge && a_extra) {
            auto& cfg = Config::Get();

            bool force = (cfg.unstackStolen && IsStolenExtraDataList(a_extra))
                      || (cfg.unstackQuest && IsQuestExtraDataList(a_extra))
                      || (cfg.unstackFavorites && IsFavoritedExtraDataList(a_extra))
                      || (cfg.unstackEquipped && IsEquippedExtraDataList(a_extra));

            if (force)
                a_merge = 1;
        }
        g_origAddExtraList(a_this, a_extra, a_merge);
    }

    void Hooks::Install() {
        Config::Get().Load();

        auto& cfg = Config::Get();
        if (cfg.debugLogging)
            SKSE::log::info("Debug logging enabled");

        SKSE::log::info("Unstacking config: stolen={}, quest={}, favorites={}, equipped={}",
            cfg.unstackStolen, cfg.unstackQuest, cfg.unstackFavorites, cfg.unstackEquipped);

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

        if (!installHook(resolveAddr(15748, 15986, 0x1E6AB0),
                reinterpret_cast<void*>(&AddExtraList_Hook),
                reinterpret_cast<void**>(&g_origAddExtraList),
                "AddExtraList"))
            return;

        SKSE::log::info("Hooks installed");
    }

}

#include "Classifiers.h"

namespace StackingPlugin {

    bool IsStolenExtraDataList(RE::ExtraDataList* a_list) {
        if (!a_list || !a_list->HasType(RE::ExtraDataType::kOwnership))
            return false;

        auto* owner = a_list->GetOwner();
        if (!owner)
            return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return false;

        if (owner == player || owner == player->GetActorBase())
            return false;

        if (owner->GetFormType() == RE::FormType::Faction) {
            auto* faction = owner->As<RE::TESFaction>();
            if (faction && player->IsInFaction(faction))
                return false;
        }

        return true;
    }

    bool IsQuestExtraDataList(RE::ExtraDataList* a_list) {
        if (!a_list || !a_list->HasType(RE::ExtraDataType::kAliasInstanceArray))
            return false;

        auto* aliasArray = a_list->GetByType<RE::ExtraAliasInstanceArray>();
        if (!aliasArray)
            return false;

        for (auto* instanceData : aliasArray->aliases) {
            if (instanceData && instanceData->alias && instanceData->alias->IsQuestObject()) {
                return true;
            }
        }

        return false;
    }

    bool IsFavoritedExtraDataList(RE::ExtraDataList* a_list) {
        if (!a_list)
            return false;

        return a_list->HasType(RE::ExtraDataType::kHotkey);
    }

    bool IsEquippedExtraDataList(RE::ExtraDataList* a_list) {
        if (!a_list)
            return false;

        return a_list->HasType(RE::ExtraDataType::kWorn) ||
               a_list->HasType(RE::ExtraDataType::kWornLeft);
    }

}

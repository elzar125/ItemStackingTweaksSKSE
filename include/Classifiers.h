#pragma once

#include <RE/Skyrim.h>

namespace StackingPlugin {

    bool IsStolenExtraDataList(RE::ExtraDataList* a_list);
    bool IsQuestExtraDataList(RE::ExtraDataList* a_list);
    bool IsFavoritedExtraDataList(RE::ExtraDataList* a_list);
    bool IsEquippedExtraDataList(RE::ExtraDataList* a_list);

}

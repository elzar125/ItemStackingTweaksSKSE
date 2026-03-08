#include "Config.h"
#include <SimpleIni.h>
#include <filesystem>

namespace StackingPlugin {

    Config& Config::Get() {
        static Config instance;
        return instance;
    }

    void Config::Load() {
        const auto iniPath = std::filesystem::path("Data") / "SKSE" / "Plugins" / "ItemStackingTweaks.ini";

        if (!std::filesystem::exists(iniPath)) {
            return;
        }

        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(iniPath.string().c_str()) < 0) {
            return;
        }

        debugLogging = ini.GetBoolValue("General", "bDebugLogging", false);
        unstackStolen = ini.GetBoolValue("Unstacking", "bUnstackStolen", true);
        unstackStolenIncludeIngredients = ini.GetBoolValue("Unstacking", "bUnstackStolenIncludeIngredients", false);
        unstackQuest = ini.GetBoolValue("Unstacking", "bUnstackQuest", true);
        unstackFavorites = ini.GetBoolValue("Unstacking", "bUnstackFavorites", true);
        unstackEquipped = ini.GetBoolValue("Unstacking", "bUnstackEquipped", true);
    }

}

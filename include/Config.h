#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace StackingPlugin {

    struct Config {
        bool debugLogging = false;
        bool unstackStolen = true;
        bool unstackQuest = true;
        bool unstackFavorites = true;
        bool unstackEquipped = true;

        static Config& Get();
        void Load();
    };

}

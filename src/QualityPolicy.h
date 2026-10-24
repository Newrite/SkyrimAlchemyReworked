#pragma once

#include "Config.h"
#include <algorithm>
#include <array>

namespace Alchemy {
    struct ProgressionState {
        float skill = 0;
        bool requiredPerk = true;
        bool advancedPerk = true;
        std::array<bool, 4> levelPerks{};
    };

    inline int MaximumQuality(const ProgressionConfig& config, const ProgressionState& state) {
        int result = 1;
        if (config.mode == ProgressionMode::Perks) {
            for (std::size_t i = 0; i < state.levelPerks.size(); ++i) {
                if (state.levelPerks[i]) result = static_cast<int>(i) + 2;
            }
        } else if (state.requiredPerk) {
            for (std::size_t i = 0; i < config.thresholds.size(); ++i) {
                const int quality = static_cast<int>(i) + 2;
                if (state.skill >= config.thresholds[i] &&
                    (quality < config.advancedFromQuality || state.advancedPerk)) result = quality;
            }
        }
        return result;
    }

    inline int OutputQuality(int base, int maximum, bool categoryBonus, bool generalBonus) {
        return (std::min)(maximum, base + static_cast<int>(categoryBonus) + static_cast<int>(generalBonus));
    }

    inline bool BonusSucceeds(float rollPercent, float chancePercent) {
        return rollPercent >= 0 && rollPercent < chancePercent;
    }
}

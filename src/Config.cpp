#include "Config.h"

#include <cmath>
#include <stdexcept>
#include <string_view>
#include <yaml-cpp/yaml.h>

namespace {
    template <class T>
    void ReadValue(const YAML::Node& section, const char* key, T& value) {
        if (!section || section.IsNull()) return;
        if (!section.IsMap()) throw std::runtime_error("configuration sections must be mappings");
        const auto node = section[key];
        if (node && !node.IsNull()) value = node.as<T>();
    }

    void ReadPerk(const YAML::Node& section, const char* key, std::string& value) {
        ReadValue(section, key, value);
        if (value.empty()) return;
        const auto split = value.find('|');
        if (split == std::string::npos || split == 0 || split + 1 == value.size()) {
            throw std::runtime_error(std::string(key) + " must be Plugin.esp|HexFormID");
        }
        auto id = std::string_view(value).substr(split + 1);
        if (id.starts_with("0x") || id.starts_with("0X")) id.remove_prefix(2);
        if (id.empty() || id.size() > 6 || id.find_first_not_of("0123456789abcdefABCDEF") != id.npos) {
            throw std::runtime_error(std::string(key) + " must use a local hex FormID (at most 6 digits)");
        }
    }

    void ReadRecipe(const YAML::Node& section, const char* key, std::string& value) {
        ReadValue(section, key, value);
        if (value.empty()) return;
        const auto split = value.find('|');
        const auto valid = [](std::string_view part) {
            return part == "common" || part == "uncommon" || part == "rare";
        };
        if (split == std::string::npos || !valid(std::string_view(value).substr(0, split)) ||
            !valid(std::string_view(value).substr(split + 1))) {
            throw std::runtime_error(std::string(key) + " must contain two rarities separated by |");
        }
    }
}

Config Config::Read(const std::filesystem::path& path) {
    const auto root = YAML::LoadFile(path.string());
    if (!root.IsMap()) throw std::runtime_error("AlchemyReworked.yaml must be a mapping");
    Config config;
    const auto debug = root["debug"];
    ReadValue(debug, "logLevel", config.debug.logLevel);
    ReadValue(debug, "flushLevel", config.debug.flushLevel);
    const auto ingredients = root["ingredients"];
    ReadValue(ingredients, "addRaritySuffix", config.ingredients.renameIngredients);
    ReadValue(ingredients, "commonSuffix", config.ingredients.commonSuffix);
    ReadValue(ingredients, "uncommonSuffix", config.ingredients.uncommonSuffix);
    ReadValue(ingredients, "rareSuffix", config.ingredients.rareSuffix);
    const auto perks = root["perks"];
    ReadPerk(perks, "level2", config.perks.level2Perk);
    ReadPerk(perks, "level3", config.perks.level3Perk);
    ReadPerk(perks, "level4", config.perks.level4Perk);
    ReadPerk(perks, "level5", config.perks.level5Perk);
    ReadPerk(perks, "potionQuality", config.perks.potionQualityPerk);
    ReadPerk(perks, "poisonQuality", config.perks.poisonQualityPerk);
    ReadPerk(perks, "allQuality", config.perks.allQualityPerk);
    ReadPerk(perks, "doubleItems", config.perks.doubleItemsPerk);
    const auto crafting = root["crafting"];
    ReadRecipe(crafting, "level1", config.crafting.level1Recipe);
    ReadRecipe(crafting, "level2", config.crafting.level2Recipe);
    ReadRecipe(crafting, "level3", config.crafting.level3Recipe);
    ReadRecipe(crafting, "level3Alt", config.crafting.level3RecipeAlt);
    ReadRecipe(crafting, "level4", config.crafting.level4Recipe);
    ReadRecipe(crafting, "level5", config.crafting.level5Recipe);
    const auto progression = root["progression"];
    std::string mode = "perks";
    ReadValue(progression, "mode", mode);
    if (mode == "skill") config.progression.mode = ProgressionMode::Skill;
    else if (mode != "perks") throw std::runtime_error("progression.mode must be perks or skill");
    if (progression && progression.IsMap()) {
        if (const auto thresholds = progression["thresholds"]; thresholds && !thresholds.IsNull()) {
            if (!thresholds.IsSequence() || thresholds.size() != 4) {
                throw std::runtime_error("progression.thresholds must contain exactly four skill levels");
            }
            for (std::size_t i = 0; i < 4; ++i) config.progression.thresholds[i] = thresholds[i].as<float>();
        }
    }
    float previous = -1;
    for (const float threshold : config.progression.thresholds) {
        if (!std::isfinite(threshold) || threshold < 0 || threshold > 100 || threshold <= previous) {
            throw std::runtime_error("skill thresholds must increase strictly within 0..100");
        }
        previous = threshold;
    }
    ReadPerk(progression, "requiredPerk", config.progression.requiredPerk);
    ReadPerk(progression, "advancedPerk", config.progression.advancedPerk);
    ReadValue(progression, "advancedFromQuality", config.progression.advancedFromQuality);
    if (config.progression.advancedFromQuality < 2 || config.progression.advancedFromQuality > 5) {
        throw std::runtime_error("advancedFromQuality must be 2..5");
    }
    if (config.progression.mode == ProgressionMode::Perks &&
        (config.perks.level2Perk.empty() || config.perks.level3Perk.empty() ||
         config.perks.level4Perk.empty() || config.perks.level5Perk.empty())) {
        throw std::runtime_error("perk progression requires all four level2..level5 bindings");
    }
    const auto bonus = root["bonus"];
    ReadPerk(bonus, "perk", config.bonus.perk);
    ReadValue(bonus, "chance", config.bonus.chance);
    if (!std::isfinite(config.bonus.chance) || config.bonus.chance < 0 || config.bonus.chance > 100 ||
        (config.bonus.chance > 0 && config.bonus.perk.empty())) {
        throw std::runtime_error("bonus.chance must be 0..100 and a nonzero chance requires bonus.perk");
    }
    return config;
}

const Config& Config::GetSingleton() {
    static const Config config = Read(R"(Data\SKSE\Plugins\AlchemyReworked.yaml)");
    return config;
}

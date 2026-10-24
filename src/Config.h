#pragma once

#include <array>
#include <filesystem>
#include <string>

enum class ProgressionMode { Perks, Skill };

struct ProgressionConfig {
    ProgressionMode mode = ProgressionMode::Perks;
    std::array<float, 4> thresholds{20, 40, 60, 80};
    std::string requiredPerk;
    std::string advancedPerk;
    int advancedFromQuality = 4;
};

struct BonusConfig {
    std::string perk;
    float chance = 0;
};

struct DebugConfig {
    std::string logLevel = "info";
    std::string flushLevel = "trace";
};

struct CobjConfig {
    std::string level1Recipe;
    std::string level2Recipe;
    std::string level3Recipe;
    std::string level3RecipeAlt;
    std::string level4Recipe;
    std::string level5Recipe;
};

struct IngredientsConfig {
    bool renameIngredients = true;
    std::string commonSuffix = "(Common)";
    std::string uncommonSuffix = "(Uncommon)";
    std::string rareSuffix = "(Rare)";
};

struct PerksConfig {
    std::string level2Perk;
    std::string level3Perk;
    std::string level4Perk;
    std::string level5Perk;
    std::string potionQualityPerk;
    std::string poisonQualityPerk;
    std::string allQualityPerk;
    std::string doubleItemsPerk;
};

class Config {
public:
    static const Config& GetSingleton();
    static Config Read(const std::filesystem::path& path);

    const DebugConfig& GetDebug() const noexcept { return debug; }
    const PerksConfig& GetPerksConfig() const noexcept { return perks; }
    const IngredientsConfig& GetIngrConfig() const noexcept { return ingredients; }
    const CobjConfig& GetCobjConfig() const noexcept { return crafting; }

    DebugConfig debug;
    PerksConfig perks;
    IngredientsConfig ingredients;
    CobjConfig crafting;
    ProgressionConfig progression;
    BonusConfig bonus;
};

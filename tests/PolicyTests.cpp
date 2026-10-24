#include "Config.h"
#include "QualityPolicy.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
    void Check(bool condition, std::string_view message) {
        if (!condition) throw std::runtime_error(std::string(message));
    }

    class ConfigFile {
    public:
        explicit ConfigFile(std::string_view yaml) {
            const auto root = std::filesystem::temp_directory_path();
            std::random_device random;
            for (unsigned attempt = 0; attempt < 100; ++attempt) {
                const auto suffix = std::to_string(random()) + "-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                directory_ = root / ("AlchemyReworked-policy-" + suffix);
                if (std::filesystem::create_directory(directory_)) break;
                directory_.clear();
            }
            Check(!directory_.empty(), "could not allocate a unique config fixture directory");
            try {
                path_ = directory_ / "AlchemyReworked.yaml";
                std::ofstream output(path_, std::ios::binary);
                output.exceptions(std::ios::badbit | std::ios::failbit);
                output.write(yaml.data(), static_cast<std::streamsize>(yaml.size()));
                output.close();
            } catch (...) {
                std::error_code ignored;
                std::filesystem::remove_all(directory_, ignored);
                throw;
            }
        }

        ~ConfigFile() {
            std::error_code ignored;
            std::filesystem::remove_all(directory_, ignored);
        }

        ConfigFile(const ConfigFile&) = delete;
        ConfigFile& operator=(const ConfigFile&) = delete;

        const std::filesystem::path& Path() const { return path_; }

    private:
        std::filesystem::path directory_;
        std::filesystem::path path_;
    };

    Config ReadConfig(std::string_view yaml) {
        // Exercise Read(path), rather than the game-relative GetSingleton(), so tests
        // are independent of the working directory and an installed MO2 profile.
        const ConfigFile file(yaml);
        return Config::Read(file.Path());
    }

    void ExpectRejected(std::string_view yaml, std::string_view message) {
        const ConfigFile file(yaml);
        bool rejected = false;
        try {
            static_cast<void>(Config::Read(file.Path()));
        } catch (const std::exception&) {
            rejected = true;
        }
        Check(rejected, message);
    }

    constexpr std::string_view legacyConfig = R"yaml(
debug:
  logLevel: info
  flushLevel: trace
perks:
  level2: Skyrim.esm|BE127
  level3: Skyrim.esm|C07CB
  level4: Skyrim.esm|C07CC
  level5: Skyrim.esm|C07CD
  potionQuality: Skyrim.esm|58216
  poisonQuality: Skyrim.esm|58217
  allQuality:
  doubleItems: Skyrim.esm|5821D
ingredients:
  addRaritySuffix: true
  commonSuffix: (Common)
  uncommonSuffix: (Uncommon)
  rareSuffix: (Rare)
crafting:
  level1: common|common
  level2: common|uncommon
  level3: common|rare
  level3Alt: uncommon|uncommon
  level4: uncommon|rare
  level5: rare|rare
)yaml";

    void LegacyProgressionUsesTheHighestOwnedGate() {
        const auto config = ReadConfig(legacyConfig);
        Check(config.progression.mode == ProgressionMode::Perks, "legacy mode changed");
        Check(config.perks.level2Perk == "Skyrim.esm|BE127", "legacy level2 binding lost");
        Check(config.perks.doubleItemsPerk == "Skyrim.esm|5821D", "legacy double item binding lost");
        Check(config.perks.allQualityPerk.empty(), "null optional perk must remain empty");
        Check(config.crafting.level3RecipeAlt == "uncommon|uncommon", "legacy recipe lost");
        Check(config.ingredients.renameIngredients, "legacy ingredient names disabled");

        Alchemy::ProgressionState state;
        Check(Alchemy::MaximumQuality(config.progression, state) == 1, "no perks must allow only quality1");
        for (std::size_t owned = 0; owned < state.levelPerks.size(); ++owned) {
            state.levelPerks.fill(false);
            state.levelPerks[owned] = true;
            const int highest = static_cast<int>(owned) + 2;
            const int maximum = Alchemy::MaximumQuality(config.progression, state);
            Check(maximum == highest, "a higher legacy perk must not require earlier perks");
            for (int lower = 1; lower <= highest; ++lower) {
                Check(Alchemy::OutputQuality(lower, maximum, false, false) == lower,
                    "a higher legacy gate must keep lower recipe qualities available");
            }
        }
    }

    void SkillProgressionHonorsThresholdsAndPerkClamps() {
        const auto config = ReadConfig("progression:\n  mode: skill\n");
        Check(config.perks.level2Perk.empty() && config.perks.level5Perk.empty(),
            "skill progression must permit absent legacy level perk mappings");
        Check(config.progression.advancedFromQuality == 4, "default advanced gate changed");

        Alchemy::ProgressionState state;
        Check(Alchemy::MaximumQuality(config.progression, state) == 1, "skill0 must allow quality1");
        for (std::size_t index = 0; index < config.progression.thresholds.size(); ++index) {
            const float threshold = 20.0F * static_cast<float>(index + 1);
            Check(config.progression.thresholds[index] == threshold, "default skill threshold changed");
            state.skill = threshold - 0.01F;
            Check(Alchemy::MaximumQuality(config.progression, state) == static_cast<int>(index) + 1,
                "quality opened before its skill threshold");
            state.skill = threshold;
            Check(Alchemy::MaximumQuality(config.progression, state) == static_cast<int>(index) + 2,
                "quality must open exactly at its skill threshold");
        }

        state.skill = 100;
        state.requiredPerk = false;
        Check(Alchemy::MaximumQuality(config.progression, state) == 1, "required perk must clamp to quality1");
        state.requiredPerk = true;
        state.advancedPerk = false;
        Check(Alchemy::MaximumQuality(config.progression, state) == 3, "advanced perk must gate quality4 and5");
        state.advancedPerk = true;
        Check(Alchemy::MaximumQuality(config.progression, state) == 5, "advanced perk must unlock high qualities");
    }

    void QualityBonusesAreCategorySpecificAndCapped() {
        const bool potionBonus = true;
        const bool poisonBonus = false;

        Check(Alchemy::OutputQuality(2, 5, potionBonus, false) == 3, "potion category bonus missing");
        Check(Alchemy::OutputQuality(2, 5, poisonBonus, false) == 2, "potion bonus leaked to poison category");
        Check(Alchemy::OutputQuality(2, 5, false, true) == 3, "general bonus missing");
        Check(Alchemy::OutputQuality(2, 5, true, true) == 4, "independent bonuses must stack");
        Check(Alchemy::OutputQuality(4, 5, true, true) == 5, "bonuses exceeded maximum quality");
        Check(Alchemy::OutputQuality(3, 3, true, true) == 3, "bonuses bypassed progression cap");
        Check(Alchemy::OutputQuality(1, 1, true, true) == 1, "bonuses bypassed required perk clamp");
    }

    void BonusProbabilityHasAnExclusiveUpperBoundary() {
        Check(Alchemy::BonusSucceeds(0, 20), "20 percent bonus must accept roll0");
        Check(Alchemy::BonusSucceeds(19.999F, 20), "20 percent bonus must accept rolls below20");
        Check(!Alchemy::BonusSucceeds(20, 20), "20 percent bonus must reject roll20");
        Check(!Alchemy::BonusSucceeds(-1, 20), "negative rolls must not grant bonuses");
        Check(!Alchemy::BonusSucceeds(0, 0), "zero chance must never grant a bonus");
        Check(Alchemy::BonusSucceeds(99.999F, 100), "100 percent bonus must accept valid rolls");
    }

    void ConfigRejectsInvalidProgressionAndBonusInputs() {
        ExpectRejected("perks: [invalid]\n", "malformed legacy section was accepted");
        ExpectRejected("perks:\n  level2: Skyrim.esm|BE127\n", "missing legacy gates were accepted");
        ExpectRejected("progression:\n  mode: unknown\n", "unknown progression mode was accepted");
        ExpectRejected("progression:\n  mode: skill\n  thresholds: [20, 40, 40, 80]\n", "duplicate thresholds were accepted");
        ExpectRejected("progression:\n  mode: skill\n  thresholds: [20, 40, 60]\n", "wrong threshold count was accepted");
        ExpectRejected("progression:\n  mode: skill\n  thresholds: [20, 40, .nan, 80]\n", "NaN threshold was accepted");
        ExpectRejected("progression:\n  mode: skill\n  thresholds: [-1, 40, 60, 80]\n", "negative threshold was accepted");
        ExpectRejected("progression:\n  mode: skill\n  thresholds: [20, 40, 60, 101]\n", "out-of-range threshold was accepted");
        ExpectRejected("progression:\n  mode: skill\nbonus:\n  perk: PathsOfEmpyrean.esp|FBA6\n  chance: 101\n", "bonus chance over100 was accepted");
        ExpectRejected("progression:\n  mode: skill\nbonus:\n  chance: 20\n", "bonus chance without a perk was accepted");

        for (const auto binding : {"Skyrim.esm", "Skyrim.esm|ZZZ", "Skyrim.esm|1234567", "|123", "Skyrim.esm|"}) {
            const std::string yaml = std::string("progression:\n  mode: skill\n  requiredPerk: '") + binding + "'\n";
            ExpectRejected(yaml, "malformed local FormID binding was accepted");
        }

        const auto valid = ReadConfig(R"yaml(
progression:
  mode: skill
  requiredPerk: PathsOfEmpyrean.esp|0x000808
  advancedPerk: PathsOfEmpyrean.esp|000813
bonus:
  perk: PathsOfEmpyrean.esp|00FBA6
  chance: 20
)yaml");
        Check(valid.progression.requiredPerk == "PathsOfEmpyrean.esp|0x000808", "valid prefixed FormID was rejected");
        Check(valid.bonus.chance == 20 && !valid.bonus.perk.empty(), "valid probabilistic perk config was not preserved");
    }
}

int main(int argc, char** argv) {
    const std::pair<std::string_view, std::function<void()>> tests[] = {
        {"legacy progression", LegacyProgressionUsesTheHighestOwnedGate},
        {"skill progression", SkillProgressionHonorsThresholdsAndPerkClamps},
        {"quality bonuses", QualityBonusesAreCategorySpecificAndCapped},
        {"bonus probability", BonusProbabilityHasAnExclusiveUpperBoundary},
        {"configuration validation", ConfigRejectsInvalidProgressionAndBonusInputs},
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS: " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        }
    }
    for (int i = 1; i < argc; ++i) {
        try {
            static_cast<void>(Config::Read(argv[i]));
            std::cout << "PASS: packaged config " << argv[i] << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: packaged config " << argv[i] << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}

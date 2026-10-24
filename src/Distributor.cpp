#include "Distributor.h"

#include <ranges>

#include "Config.h"
#include "QualityPolicy.h"

using namespace RE;
using namespace RE::BSScript;
using namespace REL;
using namespace SKSE;

namespace {

    struct EffectPotions {
        AlchemyItem* level1{};
        AlchemyItem* level2{};
        AlchemyItem* level3{};
        AlchemyItem* level4{};
        AlchemyItem* level5{};
    };

    struct CobjMetadata {
        int targetIngredientLevel;
        int potionMinLevel;
        IngredientItem* ingr1;
        IngredientItem* ingr2;
        AlchemyItem* originalPotion;
        std::uint16_t originalCount;
    };

    inline BGSKeyword* alchemyKeyword;
    inline BGSKeyword* level1Keyword;
    inline BGSKeyword* level2Keyword;
    inline BGSKeyword* level3Keyword;
    inline BGSKeyword* level4Keyword;
    inline BGSKeyword* level5Keyword;
    inline std::map<EffectSetting*, EffectPotions> potionsByEffect = {};
    inline std::map<BGSConstructibleObject*, CobjMetadata> constructibleMetadata = {};

    typedef std::vector<IngredientItem*> IngredientArr;
    inline IngredientArr commonIngredients = {};
    inline IngredientArr uncommonIngredients = {};
    inline IngredientArr rareIngredients = {};
    inline IngredientArr emptyIngr = {};

    inline BGSPerk* level2Perk;
    inline BGSPerk* level3Perk;
    inline BGSPerk* level4Perk;
    inline BGSPerk* level5Perk;
    inline BGSPerk* potionQualityPerk;
    inline BGSPerk* poisonQualityPerk;
    inline BGSPerk* allQualityPerk;
    inline BGSPerk* doubleItemsPerk;
    inline BGSPerk* requiredPerk;
    inline BGSPerk* advancedPerk;
    inline BGSPerk* bonusPerk;
    inline std::atomic_bool initialized{false};
    inline std::atomic_bool activeAlchemySession{false};
    inline std::atomic_uint64_t loadGeneration{0};

    BGSPerk* LoadPerkFromConfig(const std::string& value) {
        if (value.empty()) return nullptr;
        const auto delimiter = value.find('|');
        if (delimiter == std::string::npos) return nullptr;
        const auto plugin = value.substr(0, delimiter);
        const auto local = std::stoul(value.substr(delimiter + 1), nullptr, 16);
        return TESDataHandler::GetSingleton()->LookupForm<BGSPerk>(static_cast<FormID>(local), plugin);
    }

    inline AlchemyItem* GetEarliestLevelPotion(EffectPotions& potions, int* outLevel) {
        if (potions.level1) {
            *outLevel = 1;
            return potions.level1;
        } else if (potions.level2) {
            *outLevel = 2;
            return potions.level2;
        } else if (potions.level3) {
            *outLevel = 3;
            return potions.level3;
        } else if (potions.level4) {
            *outLevel = 4;
            return potions.level4;
        } else if (potions.level5) {
            *outLevel = 5;
            return potions.level5;
        }
        return nullptr;
    }

    inline IngredientArr& GetIngredientListForLevel(std::string level, IngredientArr& commonIngr,
                                                    IngredientArr& uncommonIngr, IngredientArr& rareIngr) {
        if (level == "common") {
            return commonIngr;
        }
        if (level == "uncommon") {
            return uncommonIngr;
        }
        if (level == "rare") {
            return rareIngr;
        }
        return emptyIngr;
    }

    inline std::pair<IngredientArr&, IngredientArr&> GetIngredientListForCrafting(std::string craftingDef,
                                                                                  IngredientArr& commonIngr,
                                                                                  IngredientArr& uncommonIngr,
                                                                                  IngredientArr& rareIngr) {
        int delimterIndex = craftingDef.find("|");
        if (delimterIndex < 0) {
            return std::make_pair(std::ref(emptyIngr), std::ref(emptyIngr));
        }
        auto firstStr = craftingDef.substr(0, delimterIndex);
        auto secondStr = craftingDef.substr(delimterIndex + 1);

        auto& firstArr = GetIngredientListForLevel(firstStr, commonIngr, uncommonIngr, rareIngr);
        auto& secondArr = GetIngredientListForLevel(secondStr, commonIngr, uncommonIngr, rareIngr);

        return std::make_pair(std::ref(firstArr), std::ref(secondArr));
    }

    inline AlchemyItem* GetHigherLevelPotion(EffectPotions& potions, int targetLevel) {
        if (targetLevel <= 1) {
            return potions.level1;
        }
        if (targetLevel == 2) {
            if (potions.level2) return potions.level2;
            return potions.level1;
        }
        if (targetLevel == 3) {
            if (potions.level3) return potions.level3;
            if (potions.level2) return potions.level2;
            return potions.level1;
        }
        if (targetLevel == 4) {
            if (potions.level4) return potions.level4;
            if (potions.level3) return potions.level3;
            if (potions.level2) return potions.level2;
            return potions.level1;
        }
        if (targetLevel >= 5) {
            if (potions.level5) return potions.level5;
            if (potions.level4) return potions.level4;
            if (potions.level3) return potions.level3;
            if (potions.level2) return potions.level2;
            return potions.level1;
        }
        return nullptr;
    }

    inline bool HasKnownEffectInIngredient(EffectSetting* effect, IngredientItem* item) {
        auto index = -1;

        for (int i = 0; i < item->effects.size(); i++) {
            auto ingrEffect = item->effects[i];
            if (ingrEffect && ingrEffect->baseEffect == effect) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            return false;
        }

        return (item->gamedata.knownEffectFlags & (1 << index)) != 0;
    }

    inline bool HasKnownEffectInIngredients(BGSConstructibleObject* cobj, EffectSetting* effect) {
        if (!constructibleMetadata.contains(cobj)) {
            return false;
        }
        auto metadata = constructibleMetadata[cobj];
        if (!metadata.ingr1 || !metadata.ingr2) {
            return false;
        }
        auto potion = cobj->createdItem ? cobj->createdItem->As<AlchemyItem>() : nullptr;
        if (!potion || potion->effects.empty() || !potion->effects[0] || !potion->effects[0]->baseEffect) {
            return false;
        }
        auto potionEffect = effect;

        if (!HasKnownEffectInIngredient(potionEffect, metadata.ingr1) ||
            !HasKnownEffectInIngredient(potionEffect, metadata.ingr2)) {
            return false;
        }

        return true;
    }

    inline void CreateRecipes(std::vector<IngredientItem*>& first, std::vector<IngredientItem*>& second,
                              AlchemyItem* potion, int targetLevel, int potionMinLevel) {
        if (!first.size() || !second.size()) {
            return;
        }
        // to avoid creating duplicate recipes when first = second, e.g. common + common
        std::set<std::pair<FormID, FormID>> createdForFormIds;
        const auto factory = IFormFactory::GetConcreteFormFactoryByType<BGSConstructibleObject>();
        auto playerRef = PlayerCharacter::GetSingleton();
        const auto dataHandler = TESDataHandler::GetSingleton();

        for (int i = 0; i < first.size(); i++) {
            auto ingr1 = first.at(i);

            for (int k = 0; k < second.size(); k++) {
                auto ingr2 = second.at(k);
                if (ingr1 == ingr2) {
                    continue;
                }

                auto ingrFormId = ingr1->GetFormID();
                auto secIngrFormId = ingr2->GetFormID();

                auto key = std::minmax(ingrFormId, secIngrFormId);
                // create recipe if not created already
                if (!createdForFormIds.contains(key)) {
                    createdForFormIds.insert(key);
                    auto obj = factory ? factory->Create() : nullptr;

                    if (obj) {
                        auto baseEffect = potion->effects[0]->baseEffect;
                        auto baseEffectName =
                            baseEffect->GetFullName() ? baseEffect->GetFullName() : baseEffect->GetFormEditorID();

                        log::info("Level {} Recipe: {}, ingr1: {}, ingr2: {}", targetLevel, baseEffectName,
                                  ingr1->GetFullName(), ingr2->GetFullName());
                        obj->benchKeyword = alchemyKeyword;
                        obj->requiredItems.AddObjectToContainer(ingr1, 1, nullptr);
                        obj->requiredItems.AddObjectToContainer(ingr2, 1, nullptr);
                        obj->createdItem = potion;
                        obj->data.numConstructed = 1;

                        auto ingr1Cond = new TESConditionItem;
                        ingr1Cond->next = nullptr;
                        ingr1Cond->data.comparisonValue.f = 1.0f;
                        ingr1Cond->data.functionData.function = FUNCTION_DATA::FunctionID::kGetItemCount;
                        ingr1Cond->data.flags.opCode = CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
                        ingr1Cond->data.functionData.params[0] = ingr1;

                        auto ingr2Cond = new TESConditionItem;
                        ingr2Cond->next = ingr1Cond;
                        ingr2Cond->data.comparisonValue.f = 1.0f;
                        ingr2Cond->data.functionData.function = FUNCTION_DATA::FunctionID::kGetItemCount;
                        ingr2Cond->data.flags.opCode = CONDITION_ITEM_DATA::OpCode::kGreaterThanOrEqualTo;
                        // ingr2Cond->data.flags.isOR = true;
                        ingr2Cond->data.functionData.params[0] = ingr2;

                        auto playerCond = new TESConditionItem;
                        playerCond->next = ingr2Cond;
                        playerCond->data.comparisonValue.f = 0.0f;
                        playerCond->data.functionData.function = FUNCTION_DATA::FunctionID::kGetIsReference;
                        playerCond->data.functionData.params[0] = playerRef;
                        obj->conditions.head = playerCond;

                        constructibleMetadata[obj].potionMinLevel = potionMinLevel;
                        constructibleMetadata[obj].targetIngredientLevel = targetLevel;
                        constructibleMetadata[obj].ingr1 = ingr1;
                        constructibleMetadata[obj].ingr2 = ingr2;
                        constructibleMetadata[obj].originalPotion = potion;
                        constructibleMetadata[obj].originalCount = 1;

                        dataHandler->GetFormArray<BGSConstructibleObject>().push_back(obj);
                    }
                }
            }
        }
    }


    bool HasPerk(PlayerCharacter* player, BGSPerk* perk) {
        return perk && player->HasPerk(perk);
    }

    void ResetRecipes() {
        for (auto& [recipe, metadata] : constructibleMetadata) {
            recipe->createdItem = metadata.originalPotion;
            recipe->data.numConstructed = metadata.originalCount;
            if (recipe->conditions.head) recipe->conditions.head->data.comparisonValue.f = 0;
        }
    }

    void PrepareRecipes() {
        ResetRecipes();
        auto* player = PlayerCharacter::GetSingleton();
        if (!player) return;
        const auto& config = Config::GetSingleton();
        Alchemy::ProgressionState state;
        state.skill = player->GetBaseActorValue(ActorValue::kAlchemy);
        state.requiredPerk = !requiredPerk || HasPerk(player, requiredPerk);
        state.advancedPerk = !advancedPerk || HasPerk(player, advancedPerk);
        state.levelPerks = {HasPerk(player, level2Perk), HasPerk(player, level3Perk),
                            HasPerk(player, level4Perk), HasPerk(player, level5Perk)};
        const int maximum = Alchemy::MaximumQuality(config.progression, state);
        for (auto& [recipe, metadata] : constructibleMetadata) {
            if (metadata.potionMinLevel > maximum) continue;
            auto* original = metadata.originalPotion;
            auto* effect = original->effects[0]->baseEffect;
            if (!HasKnownEffectInIngredients(recipe, effect)) continue;
            const bool categoryBonus = HasPerk(player, original->IsPoison() ? poisonQualityPerk : potionQualityPerk);
            const int outputLevel = Alchemy::OutputQuality(metadata.potionMinLevel, maximum,
                                                           categoryBonus, HasPerk(player, allQualityPerk));
            if (auto* output = GetHigherLevelPotion(potionsByEffect.at(effect), outputLevel)) {
                recipe->createdItem = output;
            }
            recipe->data.numConstructed = HasPerk(player, doubleItemsPerk) ? 2 : metadata.originalCount;
            if (recipe->conditions.head) recipe->conditions.head->data.comparisonValue.f = 1;
        }
    }

    class EventHandler final : public BSTEventSink<TESFurnitureEvent>,
                               public BSTEventSink<ItemCrafted::Event> {
    public:
        static EventHandler* GetSingleton() {
            static EventHandler self;
            return &self;
        }

        BSEventNotifyControl ProcessEvent(const TESFurnitureEvent* event,
                                           BSTEventSource<TESFurnitureEvent>*) override {
            if (!event || !event->actor || !event->actor->IsPlayerRef() || !event->targetFurniture ||
                !event->targetFurniture->HasKeyword(alchemyKeyword)) return BSEventNotifyControl::kContinue;
            const bool entering = event->type == TESFurnitureEvent::FurnitureEventType::kEnter;
            activeAlchemySession = entering;
            const auto generation = loadGeneration.load();
            SKSE::GetTaskInterface()->AddTask([entering, generation] {
                if (generation != loadGeneration.load()) return;
                if (entering && activeAlchemySession.load()) {
                    PrepareRecipes();
                    SKSE::GetTaskInterface()->AddUITask([generation] {
                        if (generation != loadGeneration.load() || !activeAlchemySession.load()) return;
                        auto* ui = UI::GetSingleton();
                        auto menu = ui ? ui->GetMenu<CraftingMenu>() : nullptr;
                        auto* submenu = menu ? skyrim_cast<CraftingSubMenus::ConstructibleObjectMenu*>(
                                                   menu->GetCraftingSubMenu()) : nullptr;
                        if (submenu) submenu->UpdateConstructibleList();
                    });
                } else if (!activeAlchemySession.load()) ResetRecipes();
            });
            return BSEventNotifyControl::kContinue;
        }

        BSEventNotifyControl ProcessEvent(const ItemCrafted::Event* event,
                                           BSTEventSource<ItemCrafted::Event>*) override {
            if (!event || !event->item || !activeAlchemySession.load() || !bonusPerk) {
                return BSEventNotifyControl::kContinue;
            }
            auto* player = PlayerCharacter::GetSingleton();
            auto* ui = UI::GetSingleton();
            if (!player || !ui || !HasPerk(player, bonusPerk)) return BSEventNotifyControl::kContinue;
            auto menu = ui->GetMenu<CraftingMenu>();
            auto* submenu = menu ? skyrim_cast<CraftingSubMenus::ConstructibleObjectMenu*>(
                                       menu->GetCraftingSubMenu()) : nullptr;
            if (!submenu || submenu->currentIndex >= submenu->recipes.size()) return BSEventNotifyControl::kContinue;
            auto* recipe = submenu->recipes[submenu->currentIndex].constructibleObject;
            if (!recipe || !constructibleMetadata.contains(recipe) || recipe->createdItem != event->item) {
                return BSEventNotifyControl::kContinue;
            }
            // Completion sends this event before refreshing the selected recipe in SE, AE and VR.
            static std::mt19937 random{std::random_device{}()};
            static std::mutex randomMutex;
            float roll;
            {
                std::scoped_lock lock(randomMutex);
                roll = std::uniform_real_distribution<float>(0, 100)(random);
            }
            if (!Alchemy::BonusSucceeds(roll, Config::GetSingleton().bonus.chance)) {
                return BSEventNotifyControl::kContinue;
            }
            const auto formID = event->item->GetFormID();
            const auto count = recipe->data.numConstructed;
            const auto generation = loadGeneration.load();
            SKSE::GetTaskInterface()->AddTask([formID, count, generation] {
                if (generation != loadGeneration.load() || count == 0) return;
                auto* target = PlayerCharacter::GetSingleton();
                auto* output = TESForm::LookupByID<AlchemyItem>(formID);
                if (target && output) {
                    target->AddObjectToContainer(output, nullptr, count, nullptr);
                    log::debug("Eureka: bonus {} x {}", formID, count);
                }
            });
            return BSEventNotifyControl::kContinue;
        }
    };
}  // namespace

void AlchmeyDistributor::Initialize() {
    if (initialized.exchange(true)) return;
    const auto dataHandler = TESDataHandler::GetSingleton();
    if (!dataHandler) {
        log::error("No TESDataHandler; alchemy initialization aborted");
        return;
    }

    alchemyKeyword = TESForm::LookupByID<BGSKeyword>(0x0004F6E6);
    auto craftableKeyword = dataHandler->LookupForm<BGSKeyword>(0x800, "AlchemyReworked.esp");

    level1Keyword = dataHandler->LookupForm<BGSKeyword>(0x801, "AlchemyReworked.esp");
    level2Keyword = dataHandler->LookupForm<BGSKeyword>(0x802, "AlchemyReworked.esp");
    level3Keyword = dataHandler->LookupForm<BGSKeyword>(0x803, "AlchemyReworked.esp");
    level4Keyword = dataHandler->LookupForm<BGSKeyword>(0x804, "AlchemyReworked.esp");
    level5Keyword = dataHandler->LookupForm<BGSKeyword>(0x805, "AlchemyReworked.esp");

    auto commonIngrKeyword = dataHandler->LookupForm<BGSKeyword>(0x806, "AlchemyReworked.esp");
    auto uncommonIngrKeyword = dataHandler->LookupForm<BGSKeyword>(0x807, "AlchemyReworked.esp");
    auto rareIngrKeyword = dataHandler->LookupForm<BGSKeyword>(0x808, "AlchemyReworked.esp");


    const auto& config = Config::GetSingleton();
    const auto& perks = config.GetPerksConfig();
    const std::array<std::pair<const std::string*, BGSPerk**>, 11> bindings{{
        {&perks.level2Perk, &level2Perk}, {&perks.level3Perk, &level3Perk},
        {&perks.level4Perk, &level4Perk}, {&perks.level5Perk, &level5Perk},
        {&perks.potionQualityPerk, &potionQualityPerk}, {&perks.poisonQualityPerk, &poisonQualityPerk},
        {&perks.allQualityPerk, &allQualityPerk}, {&perks.doubleItemsPerk, &doubleItemsPerk},
        {&config.progression.requiredPerk, &requiredPerk},
        {&config.progression.advancedPerk, &advancedPerk}, {&config.bonus.perk, &bonusPerk}
    }};
    for (const auto& [name, destination] : bindings) {
        *destination = LoadPerkFromConfig(*name);
        if (!name->empty() && !*destination) {
            log::error("Configured perk {} did not resolve; initialization aborted before changing furniture", *name);
            return;
        }
    }
    if (!alchemyKeyword || !craftableKeyword || !level1Keyword || !level2Keyword || !level3Keyword ||
        !level4Keyword || !level5Keyword || !commonIngrKeyword || !uncommonIngrKeyword || !rareIngrKeyword) {
        log::error("Missing AlchemyReworked.esp keywords; initialization aborted before changing furniture");
        return;
    }
    for (auto& furn : dataHandler->GetFormArray<TESFurniture>()) {
        if (furn && furn->HasKeyword(alchemyKeyword)) {
            log::info("Overrding furniture {}", furn->GetFullName());
            furn->workBenchData.benchType = TESFurniture::WorkBenchData::BenchType::kCreateObject;
        }
    }

    // categorize and store ingredients by rarity
    for (auto ingredientItem : dataHandler->GetFormArray<IngredientItem>()) {
        if (!ingredientItem) {
            continue;
        }

        BGSKeyword* keyword;

        if (ingredientItem->HasKeyword(commonIngrKeyword)) {
            keyword = commonIngrKeyword;
            commonIngredients.push_back(ingredientItem);
            if (config.GetIngrConfig().renameIngredients) {
                ingredientItem->fullName = BSFixedString(std::string(ingredientItem->fullName.c_str()) + " " +
                                                         config.GetIngrConfig().commonSuffix);
            }
        } else if (ingredientItem->HasKeyword(uncommonIngrKeyword)) {
            keyword = uncommonIngrKeyword;
            uncommonIngredients.push_back(ingredientItem);
            if (config.GetIngrConfig().renameIngredients) {
                ingredientItem->fullName = BSFixedString(std::string(ingredientItem->fullName.c_str()) + " " +
                                                         config.GetIngrConfig().uncommonSuffix);
            }
        } else if (ingredientItem->HasKeyword(rareIngrKeyword)) {
            keyword = rareIngrKeyword;
            rareIngredients.push_back(ingredientItem);
            if (config.GetIngrConfig().renameIngredients) {
                ingredientItem->fullName = BSFixedString(std::string(ingredientItem->fullName.c_str()) + " " +
                                                         config.GetIngrConfig().rareSuffix);
            }
        } else {
            // Non-keyworded ingredients are uncommon
            // TODO: make conf option for this
            uncommonIngredients.push_back(ingredientItem);
            keyword = uncommonIngrKeyword;
            if (config.GetIngrConfig().renameIngredients) {
                ingredientItem->fullName = BSFixedString(std::string(ingredientItem->fullName.c_str()) + " " +
                                                         config.GetIngrConfig().uncommonSuffix);
            }
        }

        if (keyword) {
            log::info("Ingredient: {}, {}", ingredientItem->GetFullName(), keyword->GetFormEditorID());
        }
    }

    // Categorize and store potions & poisons by rarity
    for (auto alchItem : dataHandler->GetFormArray<AlchemyItem>()) {
        if (!alchItem) {
            continue;
        }
        if (!alchItem->HasKeyword(craftableKeyword)) {
            continue;
        }
        // skip multi-effect potions
        if (alchItem->effects.size() != 1 || !alchItem->effects[0] || !alchItem->effects[0]->baseEffect) {
            continue;
        }
        int level = 0;
        auto potionEffect = alchItem->effects[0]->baseEffect;
        if (alchItem->HasKeyword(level1Keyword)) {
            level = 1;
            potionsByEffect[potionEffect].level1 = alchItem;
        } else if (alchItem->HasKeyword(level2Keyword)) {
            level = 2;
            potionsByEffect[potionEffect].level2 = alchItem;
        } else if (alchItem->HasKeyword(level3Keyword)) {
            level = 3;
            potionsByEffect[potionEffect].level3 = alchItem;
        } else if (alchItem->HasKeyword(level4Keyword)) {
            level = 4;
            potionsByEffect[potionEffect].level4 = alchItem;
        } else if (alchItem->HasKeyword(level5Keyword)) {
            level = 5;
            potionsByEffect[potionEffect].level5 = alchItem;
        }
        if (level != 0) {
            log::info("Processing {}, isPoison: {}, Alchemy Level: {}", alchItem->GetFullName(), alchItem->IsPoison(),
                      level);
        } else {
            log::info("Skipping {}, isPoison: {}, No Alch level assigned", alchItem->GetFullName(),
                      alchItem->IsPoison());
        }
    }

    // create cobj objects
    const auto factory = IFormFactory::GetConcreteFormFactoryByType<BGSConstructibleObject>();
    auto playerRef = PlayerCharacter::GetSingleton();

    for (const auto& data : potionsByEffect) {
        auto effect = data.first;
        auto potions = data.second;

        // int minLevel = 0;
        // auto minPotion = GetEarliestLevelPotion(potions, &minLevel);
        // if (!minPotion || minLevel == 0) {
        //     continue;
        // }

        auto filterEffect = [effect](IngredientItem* ingr) {
            for (const auto ingrEffect : ingr->effects) {
                if (ingrEffect->baseEffect == effect) {
                    return true;
                }
            }
            return false;
        };

        IngredientArr effectCommonIngredients;
        std::copy_if(commonIngredients.begin(), commonIngredients.end(), std::back_inserter(effectCommonIngredients),
                     filterEffect);

        IngredientArr effectUncommonIngredients;
        std::copy_if(uncommonIngredients.begin(), uncommonIngredients.end(),
                     std::back_inserter(effectUncommonIngredients), filterEffect);

        IngredientArr effectRareIngredients;
        std::copy_if(rareIngredients.begin(), rareIngredients.end(), std::back_inserter(effectRareIngredients),
                     filterEffect);

        // ingredient rules:
        // common + common = level 1
        // common + uncommon = level 2
        // common + rare = level 3
        // uncommon + uncommon = level 3
        // uncommon + rare = level 4
        // rare + rare = level 5

        if (potions.level1) {
            auto lists = GetIngredientListForCrafting(config.GetCobjConfig().level1Recipe, effectCommonIngredients,
                                                      effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(lists.first, lists.second, potions.level1, 1, 1);
        }
        if (potions.level2) {
            auto lists = GetIngredientListForCrafting(config.GetCobjConfig().level2Recipe, effectCommonIngredients,
                                                      effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(lists.first, lists.second, potions.level2, 2, 2);
        }
        if (potions.level3) {
            auto lists = GetIngredientListForCrafting(config.GetCobjConfig().level3Recipe, effectCommonIngredients,
                                                      effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(lists.first, lists.second, potions.level3, 3, 3);
            // alt
            auto listsAlt =
                GetIngredientListForCrafting(config.GetCobjConfig().level3RecipeAlt, effectCommonIngredients,
                                             effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(listsAlt.first, listsAlt.second, potions.level3, 3, 3);
        }
        if (potions.level4) {
            auto lists = GetIngredientListForCrafting(config.GetCobjConfig().level4Recipe, effectCommonIngredients,
                                                      effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(lists.first, lists.second, potions.level4, 4, 4);
        }
        if (potions.level5) {
            auto lists = GetIngredientListForCrafting(config.GetCobjConfig().level5Recipe, effectCommonIngredients,
                                                      effectUncommonIngredients, effectRareIngredients);
            CreateRecipes(lists.first, lists.second, potions.level5, 5, 5);
        }
    }

    log::info("Total potions: {}", potionsByEffect.size());
    log::info("Total ingredients: {}", commonIngredients.size() + uncommonIngredients.size() + rareIngredients.size());
    log::info("Total recipes: {}", constructibleMetadata.size());

    if (auto* events = ScriptEventSourceHolder::GetSingleton()) {
        events->AddEventSink<TESFurnitureEvent>(EventHandler::GetSingleton());
    }
    if (auto* crafted = ItemCrafted::GetEventSource()) crafted->AddEventSink(EventHandler::GetSingleton());

    log::info("Initialization Completed");
}

void AlchmeyDistributor::ResetSession() {
    ++loadGeneration;
    activeAlchemySession = false;
    if (initialized.load()) ResetRecipes();
}

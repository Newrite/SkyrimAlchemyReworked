# Alchemy Reworked — Newrite fork

Recipe-based Skyrim alchemy, originally by Alexey Svetliakov. This fork adds
optional Paths of Empyrean progression while retaining the existing YAML format
and perk-based configuration for other overhauls.

## Build with xmake

Requires Windows x64, C++23-capable MSVC, xmake 3.0.8+, and a local checkout of
[alandtse/CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG), branch `ng`.
The library is used as-is; it is not downloaded or edited by this project.

```powershell
xmake f -y -m releasedbg --commonlib="S:\Programming\CommonLibSSE-NG"
xmake -y AlchemyReworked
xmake -y AlchemyReworked.Tests
xmake run AlchemyReworked.Tests
powershell -NoProfile -File scripts/Package.ps1
```

The default CommonLib location is the sibling `../CommonLibSSE-NG`; the option
above or environment variable `CommonLibSSE-NG` overrides it. xmake manages the
yaml-cpp and CommonLib dependencies. CMake and vcpkg are not used by this build.
SE/AE/VR support comes from CommonLib's enabled runtime options. Compilation and
static binary inspection do not constitute a gameplay test on all runtimes.

## Installation

Keep the original Alchemy Reworked ESP, KID files and its requirements enabled.
Install this fork's `AlchemyReworked.dll` under `SKSE/Plugins` **instead of** the
old DLL, preferably as a separate overriding MO2 mod. Do not load two differently
named copies of the DLL. The plugin waits for KID to finish before creating recipes.

For Paths of Empyrean, also install the YAML and ESP from
`contrib/Distribution/Paths of Empyrean`. The supplied ESP has Russian perk
descriptions and targets PoE 1.2. Load it after PoE and any other alchemy-perk
overrides. Its descriptions assume the accompanying default YAML; custom thresholds
or bonuses need corresponding text edits.

For other overhauls retain their existing YAML. `progression.mode` defaults to
`perks`; all four `perks.level2` through `level5` must resolve. In `skill` mode
these four bindings may be empty. Malformed configuration or unresolved configured
perks are logged and rejected before the mod changes furniture.

## PoE behavior

| Base Alchemy | Quality | Requirement |
| --- | --- | --- |
| Any | I | None |
| 20 | II | Prodigy, first rank |
| 40 | III | Prodigy, first rank |
| 60 | IV | Prodigy, second rank |
| 80 | V | Prodigy, second rank |

White Witchery raises potion quality by one, Wormwood Disciple raises poison
quality by one, and Purity adds one to both. Bonuses stack but cannot exceed the
unlocked maximum. This improves ingredient efficiency without modifying the shared
effects of found/shop potions. Prodigy's ingredient-discovery effects are retained.

Eureka independently grants an extra portion on 20% of **completed AR recipes**.
It does not roll when opening a menu, affect other COBJ recipes, consume extra
ingredients or award extra skill XP. The engine already awards XP using the
furniture's `usesSkill`; this mod changes only its bench type. Custom furniture
must still select Alchemy to award the intended skill XP.

Recipe outputs, counts and visibility reset from immutable metadata on each
session. Losing perks or loading a different game cannot retain upgraded recipes
or deliver a queued bonus into another save. Known effects are checked on both
ingredients. Recipe-pair deduplication uses the full unordered FormID pair.

No magnitude changes are applied globally to ALCH records. TrueFlasks configuration
is unchanged; ordinary crafted potions are not automatically deposited into flask
charges. Cooking/bomb recipes and portable-lab behavior still need gameplay testing
with the user's load order.

## Validation

Standalone tests cover progression boundaries, perk gates, capped bonuses, the
Eureka probability boundary, and legacy/new YAML validation. See
[runtime evidence](docs/runtime-evidence.md) for the successful-craft event's
control flow in SE 1.5.97, AE 1.6.1170 and VR 1.4.15, including why a custom engine
hook and a second XP award are unnecessary.

Before using a new game for a full playthrough, verify cold-start KID initialization,
all five quality tiers, known ingredients, perks gained/lost, menu refresh, save
loading while a bonus is queued, portable labs, and actual Eureka frequency.

Original code is MIT licensed; see [LICENSE](LICENSE).

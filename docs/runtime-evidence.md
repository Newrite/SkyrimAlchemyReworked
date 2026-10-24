# Completed crafting event evidence

Static inspection of the user's open IDA databases, 2026-10-25. Image base is
`0x140000000` in each database. This establishes engine control flow, not a
successful in-game load or gameplay test of this fork.

| Runtime | Input / database | COBJ completion | ItemCrafted dispatch | Subsequent recipe-list refresh |
| --- | --- | --- | --- | --- |
| SE 1.5.97.0 | `E:\Reverse\SkyrimSE.exe` / `.i64` | `0x14086E2C0` | `0x14086E41D` | `0x14086E425` |
| AE 1.6.1170.0 | `E:\Reverse\SkyrimSE1170.exe.unpacked.exe` / `.i64` | `0x14090C570` | `0x14090C7D6` | `0x14090C7DE` |
| VR 1.4.15 | `E:\Reverse\SkyrimVR.exe.i64` | `0x140899850` | `0x1408999AD` | `0x1408999B5` |

SE and AE versions were also read from the input files' version resources. VR
identity follows the supplied database and its input path `SkyrimVR.exe.1415`.

## Existing event source

`CommonLibSSE-NG/src/RE/I/ItemCrafted.cpp` exposes
`RE::ItemCrafted::GetEventSource()` using `RELOCATION_ID(50515, 51403)`.

| Runtime | Library ID | Observed getter address |
| --- | --- | --- |
| SE | 50515 | `0x1408708A0` |
| AE | 51403 | `0x14090E8A0` |
| VR | 50515 | `0x14089BE30` |

The local `tests/REL/version-1-4-15-0.csv` contains `50515,089be30`.
No new address-library ID, trampoline, or completion hook is required.

`RE::ItemCrafted::Event` contains `TESForm* item` and three boolean flags; it
does not carry a recipe, quantity, or actor. The completion code reads the
selected recipe through the submenu's array at `0x100` and index at `0x148`,
validates crafting, produces the item, sends the event synchronously, then
refreshes the list. The VR event dispatcher at `0x14089B360` directly invokes
registered sinks before returning.

The sink must therefore inspect the current selection synchronously. Use
`CraftingMenu::GetCraftingSubMenu()`, RTTI-cast to
`CraftingSubMenus::ConstructibleObjectMenu`, bounds-check `currentIndex`, and
require the selected COBJ pointer to belong to this plugin's recipe registry.
Require `recipe->createdItem == event->item`. Copy output identity and count
before deferring inventory changes; do not capture the submenu for later use.

## Engine XP already occurs

Before dispatch, each completion computes experience from the COBJ and calls
the player's `UseSkill` using the furniture's `workBenchData.usesSkill` byte
at `0xE1`. Changing `benchType` at `0xE0` to `kCreateObject` does not change it.

| Runtime | Experience calculation | Skill read | UseSkill virtual call |
| --- | --- | --- | --- |
| SE | `0x14086E325` -> `0x1403C1120` | `0x14086E32E` | `0x14086E351` |
| AE | `0x14090C5E0` -> `0x14041A860` | `0x14090C5E9` | `0x14090C610` |
| VR | `0x1408998B5` -> `0x1403D09A0` | `0x1408998BE` | `0x1408998E1` |

Raw SE/AE instructions move the computed float from `xmm0` to `xmm2` before
the virtual call; the decompiler's initially inferred integer argument was
not used as signature evidence. In SE, the amount calculation uses output
gold value multiplied by constructed quantity, then power/multiplier/base
settings.

Furniture with `usesSkill=Alchemy` already awards Alchemy XP. Do not add a
second XP award in the completion sink. Custom furniture's winning skill
setting and actual XP balance still need an in-game check. The Eureka bonus
added separately after completion does not invoke this COBJ XP path.

## Remaining runtime checks

Cold start and KID initialization; recipe visibility after queued preparation;
UI replacers and selected-index behavior; portable alchemy furniture; cancelled
crafts; repeated crafts; perk reset; loading another save with a bonus pending;
and observed Eureka frequency remain gameplay tests. Static inspection alone
does not establish those results.

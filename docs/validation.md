# Validation — 1.3.0

Date: 2026-10-25. Original fork base: upstream
`a9869456ae55f07bb4d15c5d98b5f8b43b6c9430`.
CommonLibSSE-NG: local `ng` checkout at
`12d61f3c39f7d4ba1b1ab543df2bca0e613dc747`, unchanged by this task.

## Executed

- Configured xmake 3.0.8, Windows x64 releasedbg, installed MSVC (Visual Studio 2026),
  local CommonLib checkout with SE/AE/VR enabled. yaml-cpp 0.8.0 managed by xmake.
- Built the DLL, cleaned the plugin target and rebuilt it successfully, then verified
  the no-change incremental build. CommonLib was compiled from source.
- Ran all five standalone test groups successfully. Also parsed the packaged legacy
  and PoE YAML files successfully using absolute paths (xmake runs the test from its
  output directory).
- Temporarily changed the bonus boundary from `<` to `<=`; the bonus-probability test
  failed on roll 20 as expected. Restored the source, rebuilt and passed all tests.
- PE exports checked: SKSEPlugin_Load, SKSEPlugin_Query, SKSEPlugin_Version.
  PE dependencies require only Windows and the MSVC runtime, with no separate YAML DLL.
- Reviewed successful-craft dispatch and engine XP in SE/AE/VR binaries; see
  [runtime-evidence.md](runtime-evidence.md).
- Independently reviewed the event/queue lifetime and exact COBJ registry guard.
  Added an explicit UI refresh after deferred recipe preparation.
- Read the six generated perk overrides back through houseCARL. All unrelated fields
  are preserved; only descriptions and the intended native crafting entry points
  differ. Prodigy's ingredient-discovery effects remain.
- Enabled the separate override mod in the user's VanillaLike profile. houseCARL
  reports its DLL and YAML as winners, its six perk records as winners, and no dangling
  references, missing masters or parse failures in its ESP.

The source build emitted one C4701 warning in upstream minhook `hde64.c`; the DLL
linked successfully. No custom engine hook was introduced by this fork.

## Artifacts

`AlchemyReworked.dll` SHA256:
`498CF942535FBD60E856F5762823EDCA22948C5F6A2E2A70FDF6EF286A298F67`.

The local package is produced by `scripts/Package.ps1` under
`dist-release/AlchemyReworked-1.3.0-PoE-RU.zip`. Build outputs and packages are not
committed; the ESP, YAML and reviewable perk-edit manifest are committed.

Installed MO2 folder: `houseCARL - Alchemy Reworked - Paths of Empyrean`.
The original Alchemy Reworked mod supplies its ESP/KID assets and remains enabled;
the override supplies one winning DLL, YAML and PoE compatibility ESP.

Original profile files and the previous DLL/YAML are backed up locally under
`S:\AI Agent Skyrim\backups\ar-poe-fork-20261025`. Disabling the new override restores
the original DLL/YAML, but also removes PoE progression integration. Do not restore
an old profile over later unrelated edits without comparison.

## Not executed

No game launch/load or actual crafting test was performed. Cold-start KID timing,
menu refresh with installed UI mods, portable furniture, reset/load during pending
tasks and observed Eureka frequency remain runtime checks. This build is a candidate
for gameplay validation, not evidence of an all-runtime gameplay certification.

# FILN release preparation

FILN is not yet ready for Satisfactory Mod Manager (SMM). This source handoff
contains no cooked game binaries and must not be uploaded as an installable mod.

## Current implementations

`mods/FicsItLogisticsNetwork` is the canonical native SML prototype (0.1.0).
It implements manual pairing/routing primitives. Its host tests do not establish
Unreal compilation, module startup or game compatibility.

`experiments/FILNLua` contains the newer UE4SS dispatcher and KDataForge building
definitions. These features are not implemented by the native plugin. Copying
Lua into an SML plugin or adding a descriptor will not supply a Lua runtime.
The canonical Config.lua disables route writes and automatic dispatch by default.

## Reproduce the preparation

From the outer repository directory:

```bash
bash scripts/test-filn.sh
bash scripts/test-filn-lua.sh
python3 -m unittest discover -s scripts/tests
python3 scripts/prepare-filn-release.py
```

The allowlisted source ZIP in `workspace/releases` excludes saves, checkpoints,
logs, command queues, dispatch journals, local configuration, downloaded loaders,
node_modules and build caches. It includes canonical safe configuration only.
The JSON report records file hashes and outstanding release blockers.
The archive is a source handoff, not a complete development environment or installer.
Dashboard server tooling and the upstream StarterProject stay in the workspace.

## Complete the SMM release

1. Implement the Lua dispatcher/building feature set in the native SML module,
   or establish and validate an explicitly supported SML Lua runtime integration.
   The current UE4SS proxy/bootstrap and separate host dashboard service are not
   supplied by the native plugin.
2. Install the version-matched CSS engine, MSVC/Wine and Wwise toolchain described
   in the outer README. Build FactoryEditor, resolve UHT/UBT issues and author/cook
   the missing native building assets. Keep canonical source outside StarterProject.
3. Verify exact game/SML compatibility before changing GameVersion or dependency
   ranges; do not widen the existing ranges based on portable tests.
4. Run game acceptance on a disposable save with user-controlled session loading.
   Cover registration, delivery, fueling, return, save/reload and recovery. Record
   evidence in the validation docs and keep unfinished Spec Kit tasks open.
5. Package Win64 with Alpakit Release. Linux editor binaries cannot load in the
   Windows game under Proton. Claim server support only after separate validation.
6. Inspect and test Alpakit's generated multi-target release archive; complete
   author/contact, bug reporting, dependency and version metadata for publishing.
   Upload that generated archive to ficsit.app for SMM distribution.

Official packaging and upload instructions:
- https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/ReleaseMod.html
- https://docs.ficsit.app/satisfactory-modding/latest/UploadToSMR.html

No player session, installed game files, Steam launch settings or service setup
are modified by source preparation.

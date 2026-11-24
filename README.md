# FicsIt Logistics Network (FILN)

Dynamic truck logistics for Satisfactory: providers, requesters and home depots.

**Experimental source project. No installable Satisfactory Mod Manager release exists yet.**

## Implementations

- `workspace/mods/FicsItLogisticsNetwork`: native SML C++ prototype for manual pairing and routing. Unreal compilation and runtime acceptance remain pending.
- `workspace/experiments/FILNLua`: newer UE4SS dispatcher, programmable stations, depot handling and local dashboard. This is a separate experimental runtime, not the native plugin.

The Lua runtime requires manual UE4SS and KDataForge setup. Follow its [setup and limitations](workspace/experiments/FILNLua/README.md); copying this repository into the game Mods folder will not install it. Canonical defaults disable route writes and automatic dispatch.

See the [release checklist](workspace/RELEASE.md) for the remaining SML integration, Win64 Alpakit packaging and runtime validation work. [Development setup](DEVELOPMENT.md) documents the external toolchain and StarterProject, which are not vendored here.

## Host checks

```bash
bash scripts/test-filn.sh
bash scripts/test-filn-lua.sh
python3 -m unittest discover -s scripts/tests
```

These checks do not prove game compatibility. Some Python tests require localhost sockets.

## Source scope

Game saves, runtime state, logs, local credentials, downloaded toolchains, upstream StarterProject and generated archives are excluded. Historical spec/evidence links in development notes refer to the original workspace and may not be available here.

New FILN code is GPL-3.0-only; see [LICENSE](LICENSE). Game/engine content retains its upstream ownership. Item icon provenance is recorded in `workspace/experiments/FILNLua/dashboard/icons/manifest.json`.

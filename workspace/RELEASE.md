# FILN release preparation

**Release blocked.** The source handoff is not an installable SMM mod.

Authoritative scope: [native specification](specs/014-native-completion/spec.md), [standalone station requirements](specs/014-native-completion/standalone-station-implementation.md), [consistency audit](specs/014-native-completion/consistency-audit.md), [acceptance matrix](specs/014-native-completion/acceptance.json).

Canonical native implementation: `mods/FicsItLogisticsNetwork`. Historical Lua experiment: `experiments/FILNLua` (not part of native runtime or package). Browser dashboard may run separately in read-only native mode.

## Release sequence

1. Reconcile unpublished local native compilation fixes; remove required and optional Smarter Truck Stations dependencies from native source, assets, build and package metadata. Do not copy unlicensed assets.
2. Implement FILN-owned single-resource selection for both existing solid and fluid stations using original game UI. Verify resource form, capacity, ingress, docking and multiplayer authority.
3. Resolve depot root-module authoring failure with supported native SML root discovery; author and independently reload original depot and UI assets, confirm cook inclusion.
4. Verify automatic ownership/overflow, delivery/transfer receipts, parking/fuel/recovery, read-only dashboard and performance in isolated game copies, with no Smarter Truck Stations/Lua/UE4SS runtime. Legacy conversion is external; original City and running session untouched.
5. Build matching CSS/SML Win64 client/server targets, cook with Alpakit, inspect PE, actual cooked content and imports, validate SMM profile. Do not deploy or publish until separately authorized and all release gates verified.

Run portable and Python tests, then `python3 scripts/report-native-acceptance.py` (must fail while unresolved). `python3 scripts/prepare-filn-release.py` creates only a source handoff. The package verifier enforces versioned SML-only metadata; older dependency-based archives must fail. This is necessary, not sufficient, for acceptance.

Official packaging: https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/ReleaseMod.html

## Publication compliance

See [PUBLICATION.md](PUBLICATION.md). Run `python3 scripts/check-release-compliance.py` before assembling any handoff. The local icon exporter requires shipping build, pixel/transparency, cache reuse and cold-cache frame-time evidence. A successful source check does not authorize release. Do not distribute runtime cache, extracted artwork or historical Lua executables. Verify the actual cooked contents and third-party provenance. Dashboard installation for ordinary SMM users still needs a verified distribution/lifecycle design.

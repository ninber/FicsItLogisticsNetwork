# FicsIt Logistics Network — native C++ mod

**Development branch, not a verified release.** Target: SML 3.12.1 / CSS 5.6.1-87 / game CL520140, SML-only dependency, solid and fluid logistics. Current source still requires TruckStationSettings and excludes fluid cargo; both are release blockers, not requirements. No Lua or UE4SS native runtime. All migration of older saves is external.

## Intended in-game setup (not yet runtime accepted)

1. Build a dedicated FILN depot (native cooked asset still missing).
2. Build ordinary LOAD stations for solid or fluid supply.
3. Build ordinary compatible UNLOAD stations and select exactly one resource using FILN's own native-looking selector (not implemented yet).
4. Use the normal vehicle route/autopilot controls to opt a truck into any depot. FILN adopts it automatically and redirects a second truck to a free reachable depot if necessary.

No FILN management menus, register button or dispatcher switch. Both media are required; existing fluid exclusion is unfinished implementation. Native dashboard telemetry is implemented in source, not gameplay verified. The recorded vanilla depot fallback is development-only, not a substitute for the missing dedicated asset. See the [consistency audit](../../specs/014-native-completion/consistency-audit.md).

## Build and verification

Canonical plugin: this directory. Run from the repository root:

```bash
bash scripts/test-filn.sh
bash scripts/install-filn.sh
bash scripts/headless.sh native
bash scripts/headless.sh integration
# Depot authoring and standalone selector assets must be fixed and reload verified first.
# Packaging is blocked until current scope is implemented.
# bash scripts/headless.sh package
```

The install script copies canonical source to the ignored StarterProject plugin, removing only retired canonical source files; it preserves editor assets. The portable regression workflow runs GCC and Clang. Earlier UHT/C++/link evidence predates the current branch. Actual native compile, inventory/road/fuel behavior, game acceptance and Win64 packaging remain to be verified. See [headless instructions](docs/HEADLESS.md), [validation](docs/VALIDATION.md), and [current Spec Kit tasks](../../specs/014-native-completion/tasks.md). No automatic game launch, installation or publication is part of the build.

## Authoritative console commands

```text
filn.list
filn.station <station-object-path> <descriptor-class-path>
filn.removeStation <station-object-path>
filn.depot register|remove <station-object-path>
filn.pair <vehicle-object-path> <home-object-path>
filn.unpair <vehicle-object-path>
filn.dispatch <vehicle-object-path> <provider-object-path> <requester-object-path>
filn.enable 0|1
filn.status
filn.reset <vehicle-object-path>
filn.rehome <vehicle-object-path> <replacement-depot-object-path>
filn.prepareUninstall
```

Use exact object paths from list. Explicit dispatch uses the same eligibility and reservation path as automatic dispatch; if path calculation is pending, retry when eligible. Recovery retains leases and cargo until a verified return or explicit parked recovery. Do not rewrite player-changed routes. Console commands are developer diagnostics and uninstall preparation, not normal player workflow.

## Uninstall preparation

Disable automatic dispatch, finish jobs and refueling, park all registered vehicles. Administrator preparation stops owned parked AP-on trucks without changing their routes, restores station filters and clears native registry state. If preparation fails after stopping trucks, those trucks remain AP-off. Save after success, then independently verify a disposable load with FILN absent. That verification is still pending.

License: GPL-3.0-only; upstream engine/game assets retain their ownership.

# FicsIt Logistics Network — native C++ mod

**Development branch, not a verified release.** Standalone SML 3.12.1 / CSS 5.6.1-87 / game CL520140. No dependency on Lua, UE4SS, or legacy station mods. All migration of older saves is external.

## In-game setup

1. Open a vanilla truck station. The FILN logistics entry opens management without changing the ordinary LOAD/UNLOAD labels. Select a compatible resource and register; use LOAD for providers and UNLOAD for requesters. Ordinary LOAD stations with only matching cargo can supply requests without registration.
2. Designate an empty LOAD station with no cargo belts/pipes as a rest stop. Fuel inputs may be supplied normally. Register a stopped, empty, undriven vehicle nearby. An explicit new registration may assign its home-only route and enable autopilot; unrelated routes are not taken over.
3. Enable automatic dispatch. Requesters must be empty; fresh pickups require a full vehicle-specific load. Matching retained cargo has priority; [S] warehouse restrictions apply. Roads are directed and vehicle-compatible.
4. Review the network overview for state, cargo, job endpoints, road-cache counters and recovery diagnostics. Do not treat a committed route as a completed delivery.
5. To remove an idle vehicle, park it at its assigned depot, finish refueling, disable autopilot and use the administrator unregister action. Its route and cargo remain unchanged.

A distinct authored rest-stop construction recipe and assets are not yet implemented. Existing vanilla stations are the functional rest-stop alternative. Localization keys are present, but translated catalogs and visual multiplayer behavior are unverified.

## Build and verification

Canonical plugin: this directory. Run from the repository root:

```bash
bash scripts/test-filn.sh
bash scripts/install-filn.sh
bash scripts/headless.sh native
bash scripts/headless.sh integration
bash scripts/headless.sh package
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
filn.manage <station-object-path>
filn.prepareUninstall
```

Use exact object paths from list. Explicit dispatch uses the same eligibility and reservation path as automatic dispatch; if path calculation is pending, retry when eligible. Recovery retains leases and cargo until a verified return or explicit parked recovery. Do not rewrite player-changed routes. The local management command opens the same panel; it is not required for normal station enrollment.

## Uninstall preparation

Disable automatic dispatch, finish jobs and refueling, park all registered vehicles. Administrator preparation stops owned parked AP-on trucks without changing their routes, restores station filters and clears native registry state. If preparation fails after stopping trucks, those trucks remain AP-off. Save after success, then independently verify a disposable load with FILN absent. That verification is still pending.

License: GPL-3.0-only; upstream engine/game assets retain their ownership.

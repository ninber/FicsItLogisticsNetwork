# Native FILN conversion — implementation and acceptance

Status: **IN PROGRESS / not installable**. This is a port of the existing FILN Lua experiment to the canonical `FicsItLogisticsNetwork` SML C++ plugin, not a new UE4SS loader and not a Lua bundle hidden inside a `.smod`. **No game save or running player session may be touched by source-only work.**

## What exists

- Existing native `ALogisticsSubsystem`: saved truck/bay registration, exclusive reservations, manual `filn.pair` and `filn.dispatch`, three directed route reachability checks, bounded round-robin watchdog, conservative recovery. UHT/UBT/game acceptance **not run**.
- New `FILNDispatchPolicy.h`: portable, deterministic policy for pickup and retained-cargo offers. A requester **must** supply a single explicit item filter and be empty; ordinary LOAD providers may supply one observed item type but need a full truckload; requesters must have enough capacity; retained cargo takes priority and may be partial; `[S]`↔`[S]` new jobs are rejected; storage requesters cannot receive unknown-origin retained cargo. Rejects missing fuel/authority/home readiness, drivers, busy endpoints and missing/invalid directed path costs. This is **policy code only**: it does not read game inventory or assign a game route.
- Existing native manual `Dispatch` now applies the shared `[S]` warehouse restriction before any route write. Existing in-flight recovery is unchanged.

## Port boundary: server authority and native API

**Native gameplay** should be owned by one authoritative SML subsystem; no UE4SS injector, KDataForge runtime, external HTTP daemon, terminal command inbox or browser dependency. Use C++ core logic behind a thin game-thread adapter. Never assume stock identity from an empty requester, and never treat a successful route write as proof of unloading.

Before enabling automatic scheduling, implement and **compile against version-matched headers**:

1. **Station snapshots:** stable station ID, LOAD/UNLOAD mode, power/standby, docked state, filtered item descriptor and verified inventory counts/capacities. Native UI/configuration must replace dependence on Smarter Truck Stations' `FilterItem`, or an optional adapter may import its settings without making it mandatory. An unconfigured UNLOAD station is ineligible. Keep one station sample per bounded pass and invalidate only changed snapshots.
2. **Truck snapshots:** native home claim and registration, driver status, route/autopilot ownership, exact cargo type/count, capacity for the filtered descriptor, fuel availability, active-job reservations. Never commandeer player-driven, busy or unmanaged vehicles. A single-station vehicle route is **not** a valid registration substitute merely because the old Lua prototype used it.
3. **Road-cost adapter:** real *directed* vehicle-compatible path distance, not station GUID order, Euclidean distance or waypoint count. Verify all legs before claiming endpoints. Implement lazily cached requester-specific reverse path ranking with versioned invalidation and bounded memory. Do not invoke pathfinding from inventory callbacks.
4. **Atomic dispatch:** re-audit the winning offer against current world state, reserve truck and both endpoints before route writes, read back the route and autopilot state, retain reservations on partial failures; no optimistic success. Implement separate native home→requester→home execution for retained cargo with a correct FSM (not the pickup FSM pretending a provider stop exists).
5. **Native parking/fueling asset:** create/cook a real road-connected rest stop with no cargo belt ports, native user-supplied fuel input, one vehicle per bay, verified holding/release behavior. The KDataForge descriptor/recipe is *not* an SML asset and cannot be counted as ported.
6. **Native persistence and recovery:** save stable identities, jobs, reservations, transfer evidence and versioned schema; reconcile after load. Handle orphaned routes, destroyed stations/trucks, route changes, low fuel and interruptions without silent loss of cargo or reservations. Recovery can replan only after native state evidence.
7. **Native UX:** build-gun onboarding, in-game station-item configuration and dispatch/diagnostics UI. Replace the external dashboard/Monitor service with an in-game UI if equivalent functionality is required. Telemetry must distinguish measured transfers from inferred traffic.
8. **Dedicated server and release:** server authority, save/reload and multiplayer; Windows Win64 native module cooked/packed by Alpakit Release with the **matching** CSS Unreal engine/SML/game version. Linux host tests or Linux editor binaries are not a Proton-loadable game mod.

## Acceptance gates (all must pass; no placeholder success)

| Gate | Current state | Evidence required |
| --- | --- | --- |
| C++ eligibility and deterministic selection | **PASS new policy test only** | 54 policy checks compiled and run locally using GCC and Clang; full repository script not rerun in this session, no Unreal code executed |
| C++ native adapter/UHT/UBT | **BLOCKED** | Custom CSS UE, MSVC/Wwise tools and compiled FactoryEditor with versioned APIs |
| Truck/station data parity | **NOT IMPLEMENTED** | Filtered inventory, fuel/driver/home and native route snapshots in game |
| Native auto dispatch and cargo reuse | **NOT IMPLEMENTED** | Two providers, two requesters, multiple trucks, road ranking, actual delivery and return |
| Native rest stop | **NOT IMPLEMENTED** | Place, attach to roads, stop/hold/release/refuel without Lua or KDataForge |
| Save migration and recovery | **NOT VERIFIED** | Disposable copy of existing save, native load/reload, no destroyed legacy buildings |
| Multiplayer/dedicated server | **NOT RUN** | Authoritative registration, commands, replication and recovery |
| SMM-ready `.smod` | **BLOCKED** | Win64 Alpakit release, manifest, install/uninstall and loader compatibility |

## Existing saves and legacy installation

The current experimental FILN buildings may reference KDataForge generated classes. Removing KDataForge/UE4SS before a verified native asset/data migration **may break those saves**. Preserve old files/identifiers, build migration on a disposable save first, and never remove or rewrite an installed game/mod/save automatically. Do not stop, restart or manipulate the user's active session without explicit approval; obtain a durable backup before any separately authorized interruption.

## Development commands

```bash
bash scripts/test-filn.sh       # portable C++ only
bash scripts/test-filn-lua.sh   # legacy behavior regression baseline
bash scripts/doctor.sh          # toolchain readiness (environment-dependent)
bash scripts/build.sh           # requires version-matched CSS engine
```

The unit tests validate decisions on supplied snapshots, **not** the origin, freshness, identity, or road costs of real game data. See `workspace/RELEASE.md` and `DEVELOPMENT.md` for the remaining release toolchain steps.

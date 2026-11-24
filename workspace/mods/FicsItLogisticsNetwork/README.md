> Native completion remains in progress. Current headless commands and guarded station
> configuration are in [docs/HEADLESS.md](docs/HEADLESS.md). Linux UHT/C++/link and Win64
> Turnkey detection pass; shared dispatch and station UI source are implemented; runtime acceptance remains
> open in [Spec Kit](../../specs/014-native-completion/tasks.md). This is not an installable release.

# FicsIt Logistics Network — native M0/M1 prototype

Status: source implemented, portable primitives tested; **not compiled with Unreal or verified
in game**. M0 audit is complete. M1 is partial and blocked on build/runtime validation and
custom parking assets. The authoritative full specification is
`../../CODEX_TASK_FicsIt_Logistics_Network.md`; Spec Kit artifacts are under
`../../specs/001-filn-parking-routing/`.

## What is implemented

- Native C++ library plugin, hard dependency only on SML (3.12.x); no FIN dependency.
- Server-owned, save-aware `ALogisticsSubsystem`, registered by a native world-subsystem
  bootstrap through SML's SubsystemActorManager. No Blueprint module asset required for this prototype.
- Explicit one-vehicle/one-home registration with generated persistent truck/bay/station IDs.
  Current path-node GUIDs are resolved from actual station identifiers, not station names.
- Manual assignment to real LOAD provider → real UNLOAD requester → the same home.
  Three directed, preset-compatible game path queries precede atomic vehicle/provider/requester booking.
- Stored route intent and job ID, bounded docking observations, requested home stop, rejection of
  duplicate assignments, route edits, wrong modes, broken references, autopilot errors, incomplete
  unload and a 30-minute watchdog timeout. Reservations remain held throughout recovery.
- Save schema 1; saved non-idle journeys become Recovery and autopilot is requested off.
  No automatic resume or assumption that an undock transferred cargo.

## Home bay fallback / unfinished custom buildable

A dedicated existing vanilla **LOAD truck station** is the explicit M1 home bay fallback.
It must have empty cargo inventory, disconnected cargo belts/pipes, and may have fuel.
Pair only while its owned truck is actually docked and autopilot is off. Dispatch requires
empty truck cargo. Never configure a one-station route; make real connected paths with at
least home + provider + requester. The mod does not create roads or recorded legacy targets.

This is **not** a completed `ALTNParkingBay` custom buildable: no recipe, hologram, mesh,
node configuration, unlock schematic or build-menu asset exists. The native docking station
base is abstract and requires authored configuration. Editor asset authoring and connection
verification remain open. The fallback uses existing nodes instead of spoofing them.

## Build and install

From the outer `SatisfactoryModding` folder:

```bash
bash scripts/test-filn.sh
bash scripts/install-filn.sh
```

Canonical source lives here, outside the ignored upstream checkout. The install script copies
it to `workspace/StarterProject/Mods/FicsItLogisticsNetwork`; rerun after source edits. It does
not delete target files. Do not edit the copied plugin as your source of truth.

Enter `distrobox enter satisfactory-modding`, install the matching CSS UE 5.6.1, MSVC/Wine,
Wwise SDK 2025.1.6.9117 and integration 2025.1.6.4248 as described in the outer README.
Generate the project files and run `bash scripts/build.sh` to build FactoryEditor Linux Development.
UHT, linkage and module/subsystem startup remain untested until that build runs successfully.
Local audited SML is 3.12.0 at `8eebc9e659`; header commit references game CL513885.

For the Windows game executable under Steam/Bazzite/Proton, package **Win64** through Alpakit
with matching MSVC, then copy using Alpakit's configured game path. Linux editor binaries are
not Windows game mod binaries. Enable FILN in the project if Unreal prompts and confirm the
module loads. For a Linux dedicated server, package the corresponding server target separately.
Use a disposable test save and collect the game's Saved/Logs output for `LogFILN`.

## Server operator commands

Run in the listen-host/server Unreal console (client consoles are rejected; no remote RPC
or player chat command was added). Availability of an interactive console in packaged server
builds is unverified. Use Output Log in the matching editor for the initial integration test.

```text
filn.list
filn.pair <vehicle-identifier-object-path> <home-station-identifier-object-path>
filn.dispatch <vehicle-identifier-object-path> <provider-identifier-object-path> <requester-identifier-object-path>
filn.status
filn.reset <vehicle-identifier-object-path>
```

Copy exact object paths from `filn.list`, not display names. Expected logs include `PAIRED`,
`TO_PROVIDER`, state transitions, and an explicit `RECOVERY` reason. State numbers:
0 Idle, 1 Reserved, 2 ToProvider, 3 Loading, 4 ToRequester, 5 Unloading, 6 Returning,
7 Completed (transient), 8 Recovery. `accepted` means the command was accepted, not that
cargo delivery succeeded. A returned truck becomes idle only after home docking and disabled
autopilot are observed and its cargo inventory is empty. Reset also requires this physical
home parking, empty home and disabled autopilot; ownership is retained. Recovery cargo must
be handled by the operator before dispatch can resume.

## Recovery limitations

On error, autopilot is requested off and reservations are retained. Manually recover the truck
to its original bay, stop autopilot, inspect cargo and issue `filn.reset`. No automatic towing,
retry path, unpair/destroyed-truck cleanup or player-facing recovery UI exists. A missing saved
endpoint, conflicting saved identity/reservation, or unsupported schema locks further dispatch
conservatively; reset does not silently clear that global lock. Repair/migration tools are future work.

## Scheduling and cache compatibility

There is no automatic scheduler, stock eligibility or provider cache yet (M2/M3). M1 route queries
occur only on explicit dispatch; all actor/API operations stay on the authoritative game thread.
Ownership and slot booking use indexed maps. Operator discovery uses the game's registered
lists; enrollment checks may scan registrations on command, never per frame.

A one-second watchdog checks up to 16 active jobs round-robin; idle and recovery fleets are
excluded. Actual elapsed world time drives timeout. Short dock events can be missed; no event
bridge is installed yet. Increasing fleet size increases observation latency (e.g. 1,000 active
jobs require at least 63 timer firings), so this watchdog is **not a validated scalable docking
observer**. Add native dock/undock events and bounded queued reconciliation before fleet deployment.
There is no <=1 ms/frame guarantee and no game-thread measurements.

The design keeps stable registry IDs separate from route GUIDs and graph generations. Future
lazy nearest-provider ranking must include all registered providers, key by actual requester,
resource, vehicle preset/constraints and topology/traversability revision, use directed physical
road distance, and bound memory/work. Stock, enable flags and job counters only filter the
ranking and must not invalidate it. M1's three path queries validate a selected trip; they are
not an implementation of the nearest-provider policy. See Spec Kit `plan.md` / `research.md`.

## Evidence and remaining work

[Validation](docs/VALIDATION.md), [in-game checklist](docs/IN_GAME_TEST.md), and
[architecture](docs/ARCHITECTURE.md). No runtime logs, cooked artifacts, custom bay or test map
were generated. The runbook describes a reproducible scene; it is not a prebuilt map.

License: new code GPL-3.0-only; LICENSE.txt copied verbatim from the starter project's GPLv3
license text. Module Build.cs follows the upstream Alpakit skeleton. No truck-filter reference
code or proprietary art was copied. Engine/game content retains its upstream ownership.

## Standalone scope
The native mod is world-independent. Legacy Lua/mod save conversion is external to the plugin. No legacy recipe recognition, journal/manifest reader or migration UI is shipped. Ordinary station/depot/vehicle registration, recovery and safe removal of native restrictions remain supported; runtime verification is still required.

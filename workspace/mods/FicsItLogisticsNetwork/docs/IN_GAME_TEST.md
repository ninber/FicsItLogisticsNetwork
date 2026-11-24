# M1 in-game acceptance — all NOT RUN

Prerequisites: successful UHT/UBT, matching installed SML/game, native plugin packaged for the
actual target, disposable save, server/host console and LogFILN log capture. First verify one
subsystem instance exists after new-world and loaded-world startup. Native bootstrap ordering
and deterministic saved actor reuse require runtime confirmation.

## Scene
Build three powered vanilla solid-cargo truck stations: H (home LOAD, empty cargo, no cargo
belts), P (LOAD with one resource stocked) and Q (UNLOAD with empty capacity and external
sorting/buffer wiring). Give each a genuine connected 1.2 path node. Connect directed usable
roads H→P→Q→H with the correct tractor/truck preset. Fuel one truck. Confirm a valid native
multi-stop route works manually before the FILN test; never test a home-only route.
Park/dock the truck at H and disable autopilot. Keep dock times long enough to observe at the
one-second watchdog cadence. Native path control, not legacy recorded targets, is required.

## Enrollment and first route
1. Run `filn.list`; save exact truck/station object paths and path GUIDs.
2. Run `filn.pair V H`, then `filn.status`; record PAIRED and unique truck/bay IDs.
3. Pair another truck with H and V with another home: both must reject without changing ownership.
4. Dispatch `filn.dispatch V P Q`; verify real route order and target waypoint index reset.
5. Immediately dispatch V again, then another paired truck to occupied P/Q: reject both.
6. Observe P arrival, real cargo load, departure, Q arrival, real unload, departure and H arrival.
   Capture vehicle route, target index, actual inventories, docking partner and error status.
   Logs are docking observations, not proof of transfer totals.
7. At H, verify autopilot off, physically parked, empty cargo, Idle and released endpoint slots.
   Verify the truck does not continue cycling after a game tick. Whether stopping during
   home docking safely holds the truck is a critical unverified behavior.
8. Restock P and assign a second trip: confirm departure from the stopped/docked home,
   correct route index, both transfers and return to the same bay again.

## Failure and persistence matrix
- Save/reload while Idle: original truck/home IDs and ownership remain; no duplicate manager.
- Save/reload in each active state: Recovery, no duplicate jobs, retained reservations,
  autopilot requested off. Return physically to H and reset; no other truck's reservation changes.
- Wrong LOAD/UNLOAD mode, duplicated endpoint, disconnected leg, reversed one-way leg,
  incompatible preset: reject before route assignment or reservation mutation.
- Disable power/fuel, break roads, edit vehicle route/modes, dismantle endpoint/home, destroy
  truck, manually disable autopilot: no false completion. Check diagnostic and retained locks.
- Fill Q to leave cargo on truck: on return, Recovery; never redispatch residual cargo silently.
- Add cargo to H: recovery/reject; reconnecting cargo conveyors is prohibited for fallback home.
- Client attempts: reject; listen-host and dedicated server behavior verified separately.
- Force missed/brief docking observation and short test timeout: recover, do not claim transfer.

## Evidence to retain
Game build, SML version, plugin revision, engine version, all commands, timestamps, saved/loaded
job and bay IDs, full LogFILN output, inventory before/after and route/waypoint captures.
Update VALIDATION.md with actual results and failures. Do not mark checklist items passed
based on host unit tests or readable headers.

## Native continuation acceptance (not executed)

Use a separately authorized disposable installation/save fixture. Do not install into,
restart, send input to, or migrate the active game. Copy legacy fixtures before any
conversion. Load through the normal game menu; do not use direct loadgame CLI shortcuts.

1. Configure two solid providers at unequal directed road distances and one empty requester,
   plus a second provider with a shorter depot approach but longer requester road. Enroll
   empty stopped trucks explicitly at unclaimed empty LOAD home stations. Cargo belts/pipes
   at homes must be disconnected; provide real fuel through native fuel inputs.
2. Use `filn.enable 1`. Verify the nearest eligible provider wins, route/target/AP readback
   matches intent, source/target cargo events credit actual transfers, the truck returns
   to its owned home with AP still on, holds there, and releases for a subsequent job.
3. Disable AP, add a driver, change an existing native route, reserve a station, remove fuel,
   fill a requester, or remove one resource unit below full vehicle capacity. Each relevant
   candidate must be excluded with diagnostics; ordinary trucks must remain untouched.
4. Repeat with vanilla fluid stations/Fluid Trucks. Measure native per-slot quantities and
   pipeline ingress, not assumed liter conversion. Attempt wrong-fluid and wrong-item input
   via pipes, conveyors, docking and manual inventory operations. Verify no contamination.
5. Retain a partial load at home. It must win over fresh pickups. Unknown/warehouse origin
   cannot serve a [S] requester; proven factory origin may. Resource changes while occupied,
   reserved, mixed or nonempty must reject without moving or deleting cargo.
6. Edit one-way roads and topology, then repeat. Observe cache invalidation on actual road
   changes and reuse across stock/job changes. Profile game-thread cost and graph bounds;
   portable timing is not a scheduler performance result.
7. Save/reload and restart mid loading, unloading and return. Compare identities, native
   routes, counters, cargo origin, home claims and reservations. Changed/missing endpoints
   must enter observable safe recovery. No inferred transfer may satisfy completion.
8. Test empty and actual supplied fuel inventories at an idle depot. Verify native source
   fuel decreases and vehicle fuel increases, then native driving consumes fuel normally.
9. Preview/import the copied legacy fixture, repeat import, induce a filter/preflight
   conflict, and verify rollback or explicit locked recovery with byte-equivalent resources
   and connections. Settings import alone is not removal of legacy actor dependencies.
10. Disable dispatch, then separately prepare uninstall. Save the prepared copy. Verify
    filters/markers/state cleanup and retained buildings/cargo/fuel/routes/connections.
    Load that prepared copy in an isolated installation without FILN and compare again.
11. On host and remote client, inspect original switch style, localized labels, pooled
    ordinary-station restoration, resource filtering and registration UI. Distant requests,
    unavailable descriptors and non-admin global controls must reject on the server.
12. Inspect actual Win64 archive and load using an isolated SMM profile. No source archive,
    Linux editor binary, placeholder resource or passing policy test satisfies this gate.

Record actual logs, counts and before/after evidence for each acceptance.json entry.
Full actor/depot/job migration and management visual/multiplayer acceptance remain incomplete, so do not
mark their gates passed after only the implemented settings or station controls succeed.

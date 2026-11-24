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

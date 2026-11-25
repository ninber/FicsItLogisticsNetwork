# Native dashboard bridge

The existing browser UI and model can now be run explicitly in native mode:

    python3 scripts/filn-dashboard.py --native-dir /absolute/path/to/game/Saved/FILN

Use the actual game FPaths::ProjectSavedDir()/FILN (for Proton, resolve the Windows user's Saved path, not a guessed Steam install folder). Browse manually to http://filn.localhost:8765/. No browser launch from Unreal, no Lua dispatcher, no process automation, no external test harness. Native mode rejects all POST actions and refuses Lua snapshots. Existing Lua dashboard remains available separately for the experimental branch; do not use its systemd Lua watcher for native mode.

Browser polling touches dashboard-demand.txt; native checks freshness at most once per five game seconds, samples at most two objects under a 1 ms soft work budget per 100 ms scheduler callback, publishes complete snapshots via an isolated JSON-only worker and temporary file replacement. No mutation of game actors or dispatch. Old snapshots can become stale while paused; browser already marks stale. Only actual measured inventories, ownership, reservations, native error, route, fuel and diagnostic values are published. Unknown capacities are not fabricated; capacity_samples currently empty. Existing Python request analysis will report unknown truckload. Need bounded per-resource capacity sampling and request route-evidence telemetry for full dashboard parity.

Remaining gates: Unreal compile and real native file path/serialization, actual browser display, no performance regression in City copy, native snapshot production with Lua disabled, save/world switch and teardown, full/partial telemetry and capacity/routing parity. Python fixtures test HTTP read-only behavior and rejection of Lua snapshots, not native runtime.

## Capacity follow-up

The exporter now samples actual allowed-slot capacity for one representative managed vehicle per observed class/slot-count type and each configured solid resource. One capacity calculation per callback; no matrix work spike. Inventory replacement/invalid capacity emits sampling error rather than fabricated values. Distinct constraints within one class/slot count are not separately modeled; dashboard capacity is representative, not an individual dispatch authority. Native runtime and full parity remain unverified.

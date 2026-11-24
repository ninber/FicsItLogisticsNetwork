# Codex Task — FicsIt Logistics Network (FILN)

## Objective
Design and implement an **independent, native C++ Satisfactory Mod Loader (SML) companion mod** for Satisfactory 1.2+ that dynamically dispatches tractors/trucks between single-resource providers and requesters, using the game's existing vehicle path/autopilot system. FicsIt-Networks (FIN) integration is an **optional adapter**, not a runtime dependency of the core dispatcher. Work in implementable milestones; prove route control and parking first.

Do not recreate Factorio's Logistic Train Network literally: **the delivery unit is one vehicle journey, not an exact item quantity**. Focus on reliable vehicle-flow control, logical stations, efficient scheduling, and low overhead for hundreds of vehicles.

## Nonnegotiable product rules
1. **Vehicle enrollment:** Add a buildable **LTN Parking Bay**, one bay per registered tractor/truck. It has a vehicle-path connection, one owned parking slot, and optionally fuel; it has no cargo-handling role. A valid game route containing the bay may initiate pairing; if that is unreliable, provide explicit pairing UI. **Do not rely on a single-station route**, which is invalid in the current game. A truck keeps its home-bay association while assigned to a job and must return to that bay before becoming idle/available again.
2. **Station type:** Existing truck stations use their normal LOAD/UNLOAD mode: LOAD = provider, UNLOAD = requester. Each physical station handles **one configured resource type**. No multi-item requester, combined load/unload mode, drones, or trains in the MVP. Station-side unloading filters may be integrated/implemented where required; the player remains responsible for external conveyor sorting and buffer wiring.
3. **Logical station classes/names:** User-friendly class or alias such as `IronPlate.Provider`/`IronPlate.Requester` may map to many physical stations. Keep separate **stable instance IDs** and internal game path-node GUIDs; never route merely by a display-name string. A class's **active** members contain only currently eligible stations: providers have enough relevant stock; requesters have sufficient spare capacity/demand. Use configurable on/off thresholds (hysteresis). A station may stay registered yet be inactive.
4. **Stock sensing:** Monitor one item type using an explicitly associated station buffer/container or reliable station inventory where appropriate. Allow a future external device/FIN adapter to publish stock and demand. No requirement for FIN wiring in native automatic mode. Do not infer total factory inventory by scanning arbitrary nearby objects.
5. **Job invariants:** Exactly **one truck per job** and at most **one active job per truck**. At most **one active job per station by default**, with a user-configurable `MaxConcurrentJobs` per station. Prevent duplicate simultaneous jobs for the same provider-requester pair (default 1; allow an explicit setting if necessary). Atomically reserve truck + provider slot + requester slot before assigning a route. An active job remains counted until completion/cancellation/recovery. The actual loaded amount is whatever the station transfers, not a mandated exact shipment size.
6. **Provider choice:** For a given requesting station, choose the **closest eligible provider by shortest *usable road/path distance from provider to actual requester***, not Euclidean distance or provider proximity to a depot. Respect one-way paths, disconnected networks, vehicle type/path preset, and vehicle-specific path restrictions. Among otherwise valid vehicles, favor the one with the cheapest empty trip to the selected provider; never let that secondary criterion override the nearest valid provider policy.
7. **Minimal overhead:** Native C++ scheduler; no global every-frame actor scans, no position polling per frame from this mod, no recomputation of shortest paths on every dispatch. Default dispatch pass every **5 seconds**, configurable 2–30 seconds. Use game events to update inexpensive state and a bounded reconciliation pass for correctness. Server-authoritative behavior in multiplayer/dedicated server.

## Essential feature: lazy nearest-provider cache
The expensive variable is **the requester-specific ordered list of candidate provider station GUIDs**, ranked by *road distance*. Treat it as a **lazy, versioned, bounded cache**.

- Suggested key: `(requester stable ID / path GUID, resource type, vehicle path preset, relevant road-network revision)`. The key must account for vehicle compatibility and directionality.
- Suggested value: ordered candidate records `(provider stable ID, provider path-node GUID, road-distance cost, reachability/validation metadata)`. Store **all registered compatible providers** of that resource, including those *currently* inactive, so inventory changes do not force rebuilding the ranking.
- On dispatch, read the cached ranking and **walk it from closest to farthest**, skipping candidates that are inactive, occupied/at their concurrent-job limit, or otherwise disqualified. If the first is unavailable, try the next **without pathfinding**.
- **Compute on first use only**. Prefer a **single reverse shortest-path traversal** from the requester on a directed, vehicle-compatible road-graph snapshot (e.g., Dijkstra), then look up all registered providers, instead of one pathfinder call per provider. Confirm graph edge lengths/validation semantics against the actual current SML headers. If graph snapshots cannot be used safely, employ bounded/cacheable game-API path queries as a fallback.
- **Recompute/mark stale** only when necessary: requester/provider built, dismantled, reconnected, reconfigured to a different resource/class, path network connectivity/topology or traversability changes, relevant vehicle preset/constraints change, cached source IDs become invalid, or explicit force-refresh. If repeated real route failures reveal stale data, invalidate with backoff. Adding a new provider must eventually make it eligible to appear in the ranking.
- **Do NOT invalidate merely because** stock rises/falls, a station enables/disables, a truck becomes busy/free, a job finishes, or a station's active-slot counter changes. These are cheap filters over the ordered list.
- Track path topology/traversability change counters where reliable (`UFGVehiclePathNetwork::GetNetworkPathTraversabilityChangelist()` is a candidate). The game's path-network IDs may change when topology changes; do **not** assume network ID is a persistent identifier. Treat graph generation as a versioned snapshot and rebuild lazily for affected requesters only.
- Keep a bounded LRU or memory-cap policy for cache entries; avoid eagerly storing every pair. Provide cache hits/misses, invalidation reasons, computation duration, entries/bytes, and fallback counts in diagnostics.
- Before assigning, validate **vehicle/home → provider → requester → same home bay** reachability; a cheap cached provider→requester distance does not prove the truck can leave or return. On failures, skip/bypass and recover safely.

## Native architecture
- `ULogisticsWorldSubsystem` or an equivalent **server-owned, save-aware** manager: station registry, vehicle registry, active jobs, dispatcher timer, event queue, cache coordinator. Check appropriate Unreal/SML lifecycle class and actual world/save hooks before choosing inheritance.
- `ALTNParkingBay`: route-connected buildable and per-bay owned vehicle registration / parked state. Prototype its integration with the new Satisfactory 1.2 path nodes; never spoof old `AFGDrivingTargetList` vehicle waypoints.
- `FLTNCargoStationConfig` per actual station: item descriptor, logical class, mode derived from actual station load/unload setting, linked buffer inventory, thresholds, concurrency, priority, enabled state; stable ID and game path GUID resolved internally.
- `FLTNJob`: unique job ID, vehicle, provider, requester, bay, state, actual cargo/transfer counters, timestamps, errors/retries and route intent. State machine includes `IDLE`, `RESERVED`, `TO_PROVIDER`, `LOADING`, `TO_REQUESTER`, `UNLOADING`, `RETURNING`, `COMPLETED`, `RECOVERY`.
- `FNearestProviderCache`: lazy candidate ranking and selective invalidation with safe, immutable path-graph snapshot if doing algorithm work off-thread.
- `FLTNFlowHistory`: compact fixed-size time-bucket statistics and rolling aggregates.
- Future optional FIN integration exposes high-level state, telemetry, station controls and job requests without allowing scripts to violate native job invariants.

## Current SML interfaces to verify (not guaranteed working integrations)
Inspect version-matched public headers and verify actual behavior in game; do not assume a declaration guarantees safe runtime use:
- `WheeledVehicles/FGWheeledVehicleIdentifier.h`: `GetVehicleRoute`, `SetVehicleRoute`, `SetAutopilotEnabled`, `GetCurrentTargetWaypointIndex`, `GetAutopilotErrorStatus`.
- `WheeledVehicles/FGDockingStationIdentifier.h`: `GetPathNodeGUID`, `GetStationStatus`, `GetStationName`.
- `WheeledVehicles/FGVehicleSubsystem.h`: `GetDockingStations`, `GetAllVehicles`, `FindNetworkByPathNodeGuid`; `UFGVehiclePathNetwork::FindVehiclePath` and `GetNetworkPathTraversabilityChangelist`.
- `Buildables/FGBuildableDockingStation.h`: `GetDockingPathNode`, `GetInventory`, `GetFuelInventory`, `GetIsInLoadMode`, docking state.
- `FGInventoryComponent.h`: `GetNumItems` and native item-added/item-removed delegates. Inventory callbacks may be suppressed; periodically reconcile counts.
Reference: https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/master/Source/FactoryGame/Public/WheeledVehicles
Reference filtering design: https://github.com/IKholopov/SM_TruckStationSettings/blob/main/Source/TruckStationSettings/Private/TruckStationSettings.cpp (evaluate licensing/compatibility; do not blindly copy code).

## Scheduling and resource budgets
- Maintain indexed lists/sets for idle vehicles, active requesters/providers per item type and active jobs; never perform all-pairs scans per dispatch.
- Run scheduling on a ~5-second timer. Events mark entities dirty and schedule cheap incremental updates; prohibit route-search invocation from frequent inventory callbacks.
- Time-slice costly path/candidate updates across frames. Target <=1 ms *game-thread work per frame* by budgeting tasks and measuring actual time; **do not promise** performance before profiling.
- Keep actor manipulation and game API mutations on the authoritative game thread. Only immutable copied graph data may be processed asynchronously after thread-safety verification.
- Avoid doing full-fleet scans per timer tick. Existing jobs should be event-driven with infrequent watchdog checks. Demonstrate performance using synthetic 100/500/1000 vehicles and hundreds of stations; separate game autopilot CPU cost from FILN cost.

## Station telemetry and Power Graph-like UI
Implement per-station time-series widgets visually resembling Satisfactory's **power-production graph** (visual language: clear time axis, compact multiple lines, current/min/max/rolling average, legend and selectable history window; do not copy proprietary art assets). Record **actual** items loaded/unloaded per minute, monitored buffer stock, and **estimated production or consumption per minute** only when the required measurements exist. Clearly label inferred vs measured figures.
- 60-second buckets; default display past 2 hours; optional aggregated 5-minute buckets for last 24 hours.
- Minimum/maximum over buckets in selected window; rolling averages for configurable 5/15/60 minutes; efficient ring buffers, running sums, and charts refreshed only while visible.
- A station's `actual transferred/min`, supply/demand rate, average trip time, busy time, queue/active-job utilization and warnings can be used to show bottlenecks. Avoid claiming precise theoretical capacity without machine data.
- Persist statistics compactly if practical, or explain/implement an explicit persistence policy.

## Ordered delivery milestones (Codex should actually implement/test, not just write a plan)
**M0 — Repository audit/build readiness.** Inspect existing repository before changing it; identify current SML, game/Unreal versions, build procedure, plugin dependencies and licensing. Create architecture and risk notes. If no mod repo exists, initialize a minimal SML mod skeleton. List exact missing prerequisites instead of fabricating build success.

**M1 — Parking and routing proof-of-concept.** Build/register one LTN parking bay; pair exactly one truck; assign a route to a real LOAD station, real UNLOAD station and home bay; verify docking, pause/idle and subsequent reassignment. Ensure save/load recovery. Provide an in-game test scene/checklist and logs. If custom docking-node support blocks progress, document minimal workaround and continue with an explicit registration fallback; no fake successful integration.

**M2 — Functional automatic dispatcher.** Implement one-resource station configs, active eligibility, concurrency/job exclusivity, dispatch timer, nearest eligible provider selection, full job lifecycle, route validation and error/recovery. Test two providers at different road distances, one requester, two trucks, one home bay per truck, disabled stations and capacity limits.

**M3 — Lazy path cache and scalability.** Implement the sorted provider-GUID cache, scoped invalidations, bounded graph work, memory limits and instrumentation. Add tests confirming inventory/job activity does not trigger path recalculation, while road edits/new providers do.

**M4 — Telemetry UI.** Implement power-style graph, min/max/rolling averages, per-station history, and diagnostics. Optimize graph rendering when UI hidden.

**M5 — Optional FIN interface.** Detect a compatible FIN installation; expose high-level native APIs through its officially supported connector mechanism without making the native mod dependent on FIN. If FIN is incompatible with the target game build, implement a clean adapter boundary and leave it disabled with clear documentation.

## Required acceptance tests
1. Two providers with unequal **road** distance: choose closest to the actual requester even if the farther provider is closer in straight-line distance or to a depot.
2. Closest provider runs out of stock or reaches its active-job limit: pick the next cached GUID, **zero new pathfinding** if cache still valid. Once stock returns, choose the closest again on a future job.
3. Repeated enable/disable inventory changes and jobs do not invalidate the provider ranking. Building/removing a provider or changing road topology does.
4. One truck never receives two concurrent jobs; same station and pair never exceed configurable job limits. Booking of vehicle and both stations is atomic.
5. Jobs survive save/reload, dismantled stations, broken road, fuel shortage, incomplete unload and vehicle destruction without permanently locked reservations.
6. Station with no request or inadequate supply stays inactive, but remains registered in its logical class.
7. Simulated 500 vehicles / 300 stations: collect per-dispatch durations, per-frame game-thread max time, memory and path-cache recompute counts. Report measurements, not estimates.
8. History UI displays sensible zero-traffic time segments, min/max, rolling averages and correct rates; avoids processing the chart when closed.

## Work style and deliverables
- Make incremental commits/files and leave the project in a buildable or clearly documented blocked state at each milestone.
- Prefer small, cohesive C++ modules, Unreal/SML conventions, server authority and backward-compatible save data.
- Do not silently introduce FIN as a hard dependency. Do not use legacy vehicle target-list APIs for Satisfactory 1.2+.
- Provide a README with build/run instructions for Windows game executable (including a Steam-on-Bazzite/Proton testing note), config/how-to-use instructions, test checklist, benchmark method and limitations.
- For each milestone report: files changed, behavior implemented, tests run and results, remaining blockers, and next step. **Never mark tests passed if they were not run.**

Start with **M0 and M1**, then implement subsequent milestones only after the first-route proof of concept is demonstrably functional or its blockers are clearly documented.

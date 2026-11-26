# Implementation plan: native completion

Technical context: C++20 policy plus native Unreal/SML authoritative saved subsystem. Linux
editor toolchain and Win64 Wine/MSVC target. No Lua dependency in native plugin. Spec Kit
constitution remains the untouched upstream template; project guidance and current user
requirements supply the actual safety/behavior constraints.

1. Establish build provenance and reproduce logged status without duplicate compilation.
2. Verify SDK/compiler execution, Wwise paths/banks/libraries, Vulkan container; make headless
workflows idempotent, locked and observable. Preserve all caches and existing dependencies.
3. Import PR #1 policy by pinned SHA; audit Lua/API/spec differences and add traceability.
4. Extend vanilla station persistence with native slot restrictions and safe restoration.
5. Remove all Smarter Truck Stations dependencies; implement FILN-owned single-resource selector on both station types without changing LOAD/UNLOAD labels; validate inventory/pipe ingress and capacities.
6. Add bounded dirty-state snapshots, shared dispatcher, native graph adapter and lazy
requester cache. Atomic reservations followed by native route and autopilot readback.
7. Resolve native root discovery/asset authoring, independently reload and cook depot and selector assets; verify compatible depots/refueling, automatic enrollment/overflow, measured transfers, retained-cargo FSM, recovery and save lifecycle.
8. Legacy migration is external; native source provides ordinary registration and safe uninstall only.
9. Complete read-only browser telemetry for both media, disabled-state semantics and isolated uninstall preparation/load. Add the read-only native Organizer per R11; no management controls.
10. Run pure, Unreal integration, multiplayer and isolated game acceptance separately; produce
and inspect Win64 package only after these gates. Never deploy to the active installation.

Architecture evidence: installed FGVehicleSubsystem exposes directed segment indices,
SegmentLength, per-vehicle traversability and virtual fuel lengths. Do not use volatile network
IDs as persistent identities. mIsFluidStorageInventory is a reflected protected class setting;
current station code reads it without changing the vanilla class. Fuel must use native
inventories/consumption. Physical actor replacement is deferred until a lossless mechanism
has executable evidence, rather than relying on untested class redirects.

## Native Organizer implementation (2026-11-26)
1. Native AFGHUD::OpenInteractUI / UFGInteractWidget lifecycle and local input callback; verify cooked player-mappable Enhanced Input assets separately.
2. One completed in-memory snapshot shared with existing web export; lease demand while open, no disk reread or second world scan, no cross-world state. Separate saved resource, stock and filter readiness. Keep existing JSON fields compatible.
3. Compact Vehicles rows with all cargo types including idle, actual route/current target; Stations grouped by resource identity and provider/requester columns; Diagnostics last, precise failed registry preflight and vehicle reasons.
4. Use game item names and icons, native interaction stack, cohesive custom style if verified game UI style assets cannot be reused. Check actual 1080p and 4K screenshots, long names/routes, scroll and focus. No placeholder acceptance.
5. Validate runtime performance, lifecycle, blocked state, no dispatcher mutation, web compatibility. Remote multiplayer clients require bounded authorized transport; explicitly report unavailable until implemented.

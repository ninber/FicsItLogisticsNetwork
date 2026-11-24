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
5. Implement native resource/mode UI; validate inventory/pipe ingress and capacities.
6. Add bounded dirty-state snapshots, shared dispatcher, native graph adapter and lazy
requester cache. Atomic reservations followed by native route and autopilot readback.
7. Native depots/refueling, measured transfers, retained-cargo FSM, recovery and save lifecycle.
8. Versioned legacy inventory/connections/state migration with dry-run and recovery journal.
9. Complete management UI, disabled-state semantics and isolated uninstall preparation/load.
10. Run pure, Unreal integration, multiplayer and isolated game acceptance separately; produce
and inspect Win64 package only after these gates. Never deploy to the active installation.

Architecture evidence: installed FGVehicleSubsystem exposes directed segment indices,
SegmentLength, per-vehicle traversability and virtual fuel lengths. Do not use volatile network
IDs as persistent identities. mIsFluidStorageInventory is a reflected protected class setting;
current station code reads it without changing the vanilla class. Fuel must use native
inventories/consumption. Physical actor replacement is deferred until a lossless mechanism
has executable evidence, rather than relying on untested class redirects.

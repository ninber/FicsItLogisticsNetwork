# Architecture and risk boundaries

`ULogisticsBootstrap` is a native UWorldSubsystem only in authoritative gameplay worlds;
it registers `ALogisticsSubsystem` with SML's SubsystemActorManager at world begin play.
The actor implements IFGSaveInterface explicitly and stores schema-versioned registrations,
station IDs, job intent and diagnostics. Native subsystem initialization and save order are
still subject to UHT/UBT/runtime verification. Engine-independent FILNCore primitives are
shared by adapter and host tests, not a mock reimplementation of game vehicle behavior.

A job owns truck/provider/requester reservations. The truck's home is permanently enrolled
and excluded from cargo endpoints. Rejection acquires no partial slots. After route mutation
failure, reservation is intentionally retained in Recovery. Confirmed home stop releases job
slots but never home ownership. Authoritative console commands are an operator prototype,
not remote player UI. The watchdog only observes active jobs and never scans all world actors.

Explicit station actors are not selected by aliases. Generated station instance IDs survive
save/load, while current path GUIDs resolve route destinations. Network IDs are never saved.
Topology rebuilds can replace network objects; traversability counters alone are insufficient.
Future candidate caches include inactive compatible providers and selectively invalidate only
structural/configuration/vehicle-constraint changes, not stock or dispatch activity.

M0/M1 deliberately do not implement exact cargo quantities, filtering, stock-demand logic,
priority, automatic provider selection, arbitrary buffer discovery, telemetry, cache, FIN or
player widget. Real truck transfer amounts and unload success must be verified from game
inventories. Empty cargo on home return is a necessary guard, not evidence that delivery occurred.

Risks: generated stubs do not prove behavior; dock observations can be missed; route replacement
may preserve unexpected target indices; home autopilot stop may interact with docking;
save actor references and native subsystem registration may require lifecycle adjustment;
no asset authoring/custom node support proof exists. Destroyed-reference recovery remains
operator/manual and conservatively retains locks. No automatic release that could duplicate jobs.

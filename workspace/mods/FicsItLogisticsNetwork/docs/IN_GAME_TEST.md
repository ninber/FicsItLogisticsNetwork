# Standalone FILN gameplay acceptance — NOT RUN

Prerequisites: matching CSS/SML/game, clean native Win64 build/cook and verified asset reload; no Smarter Truck Stations, Lua dispatcher or external UE4SS test harness. User-controlled disposable save; do not install, restart, automate UI or migrate original City without permission. Capture logs, timings, native route/target/AP readback, exact inventories and stable identities.

## Player workflow (run independently for solid and fluid)

1. Place dedicated FILN depot, ordinary compatible LOAD and UNLOAD stations, connect directed roads and supply actual fuel. Verify construction menu and station UI; ordinary stations remain ordinary.
2. Select exactly one compatible resource in FILN's native-looking selector. Verify server readback, no additional registration/apply, no stock-derived demand, incompatible selection visibly rejected. Try changes while nonempty, docked and reserved; ensure no loss. Verify real conveyor/pipe ingress and native docking restrictions including wrong resource.
3. Park compatible vehicle at depot, assign normal depot-only route and enable AP. Verify automatic adoption. Add second vehicle to same depot; it must travel to an unclaimed compatible reachable depot without moving owner and settle into home-only route. Repeat simultaneous claim, no-free-depot, disconnected/incompatible alternative and driver/AP-off cases.
4. Stock providers; observe automatic dispatch, nearest provider-to-requester road ranking, exact full-load/capacity, empty requester, partial retained cargo priority, [S] restrictions, actual native loading/unloading receipts and exclusive reservations. Verify physical return and hold with AP still on, subsequent job, real refuel and no synthetic fuel. For fluids measure actual native quantities and pipe behavior, not guessed liters.
5. Exercise partial transfers, mismatched/missing receipts, worker/game-thread execution, owner/significance changes, power/fuel loss, road edits, driver takeover, destroyed actors, restart and save/reload mid-job. No false delivery or duplicate ownership. Verify compatible docking for each medium.
6. Open browser manually in native read-only mode. Compare snapshot quantities, units, capacities, jobs, ownership and stale state against native observations. Profile bounded sampling in large isolated world. No automatic browser/engine UI calls or dashboard writes.
7. Verify host/remote-client selector authority and replication; listen/dedicated-server behavior separately. Test uninstall preparation and independently load disposable save without FILN after safe conversion/dismantling of mod recipe references. External migration separate; no native legacy import.
8. Inspect Win64 archive and actual cooked imports; no TruckStationSettings/Lua/UE4SS/DataForge dependency. Verify isolated SMM loading only with user authorization.

Record actual results in [acceptance matrix](../../../specs/014-native-completion/acceptance.json). Failed or absent requirements remain unresolved. Host tests, route setters, asset headers and editor compilation alone do not establish gameplay.

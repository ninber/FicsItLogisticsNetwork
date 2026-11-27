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

## Release-blocking filter regressions (#5 and #6)

These are required native acceptance checks, not evidence of success.

- Build one solid and one fluid FILN logistics station and both depot types. Confirm the logistics stations expose FILTERS and depots do not.
- Select a resource before connecting the station or registering any vehicle. Change it rapidly three times, then clear; reopen and check that the last edit remains. Keep the dropdown open for more than ten seconds during dispatcher refresh.
- In Blueprint Designer select a solid and a fluid resource on the corresponding station. Save, place two copies, change and resave the original blueprint, then clear and resave. Verify each placed copy has its own exact desired selection.
- Save/reload the world during an outstanding filter change; verify the previous active filter remains for an already reserved delivery while the latest desired filter is retained. Verify no new old-resource delivery is assigned; after undocking and release, the new resource or clear is applied.
- For a fluid station containing fluid A, choose fluid B while connected or reserved. The selection must remain editable and visibly B, while incompatible new delivery is excluded and fluid A is not deleted. Drain or disconnect incompatible fluid normally and verify B becomes operational without reselecting it. Repeat with gas and clearing.
- Check ordinary stations, inactive dispatcher, station without path nodes, and multiplayer. Keep native storage controls intact in both solid and fluid UI. No internal diagnostic messages should appear in FILTERS.
- Verify a legacy save whose configured station predates actor resource persistence is migrated only if the actor has never saved an explicit selection. Verify an explicit clear is not resurrected on reload.

Do not close either issue on portable test or compilation evidence alone.

## Organizer keyboard regression
- In normal gameplay with no other menu open, press Ctrl+Shift+L; verify Organizer opens and Escape closes it. Repeat after closing a solid and fluid station, after save reload and after respawn. Verify no duplicate Organizer windows.
- Check that typing in native text fields or the game console does not open Organizer and that pause/other active interactions remain unaffected.
- Capture FILN_ORGANIZER_INPUT, FILN_ORGANIZER_SHORTCUT and FILN_ORGANIZER_OPEN logs on failure. The console command filn.organizer is an independent diagnostic entry point; close the console after issuing it.

## Fluid fleet parking / dispatch regression
- Fill six compatible fluid depots with six assigned trucks, each on its own one-node home route. Use powered empty LOAD depots with fuel; include a stopped truck physically within native docking range but not yet docked. No spare depot. Confirm native docking readback precedes Held, refueling and dispatch. Verify an actual crude oil delivery from a full provider to an empty configured requester; preserve cargo, ownership and native AP opt-out.
- Repeat with driver present, AP off, occupied depot, outside docking range, and native CanDock rejection. No forced docking, synthetic cargo/fuel or fabricated delivery. Organizer vehicle row must expose parking reason; capture telemetry and actual native route and fuel state.

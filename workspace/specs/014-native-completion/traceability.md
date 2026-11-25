# Current requirement traceability — 2026-11-25

Current scope: [spec](spec.md), [standalone station requirements](standalone-station-implementation.md). See [consistency audit](consistency-audit.md) and [acceptance matrix](acceptance.json). All in-scope runtime/release gates remain unresolved. Earlier reports and task checkmarks are historical, not current proof.

| Requirement | Source / current state | Remaining gate |
|---|---|---|
| R01 build | Local Codex reports editor compile/link with unpublished unity fixes; GCC/Clang CI baseline passed | Integrate fixes, rebuild current source, SDK/UHT/Win64 |
| R02 stations | ConfigureStation/ChangeStationFilters, currently rejects fluids and reads external filters | Own compatible persistent selection, conveyor/pipe ingress |
| R03 selector | Old management menu removed; external FilterItem remains | Original FILN selector and multiplayer/readback for both media |
| R04 dispatch | LogisticsDispatch, FILNDispatchPolicy; solid-only restrictions remain | Actual solid/fluid capacities, eligibility and delivery |
| R05 routing | FILNRoadCache, directed checks | Native distance/order/invalidation/performance |
| R06 transfers | FILNCore, FILNDockReceipts, FILNTransferLedger | Native result, thread and inventory reconciliation |
| R07 home/fuel | LogisticsPlayerWorkflow, FILNParking, LogisticsFuel, author script | Dedicated cooked depot, compatible enrollment/overflow/refuel and park |
| R08 storage | FILNDispatchPolicy | Both media, retained provenance |
| R09 migration | External save tools only | Not native scope; copies only, verify separately |
| R10 uninstall | Prepare/restore source | Independent mod-absent reload, recipe reference handling |
| R11 telemetry | LogisticsTelemetry, native browser; solid-only assumptions | Actual read-only snapshot, both media, units/performance |
| R12 persistence | Saved IDs/config/leases, recovery | Save/restart, destruction, takeover, reload |
| R13 release | Verifier now SML-only; manifest/prerequisites still third-party | Remove dependency/imports, author/cook/reload, Win64 artifact + gameplay |

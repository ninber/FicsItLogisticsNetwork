# Requirement traceability (2026-11-24)

Lua sources: experiments/FILNLua/Scripts; native source: mods/FicsItLogisticsNetwork.
PASS below names the actual verification scope, never implied game acceptance.

| Requirement | Lua reference | Native implementation | Automated coverage | Actual status/blocker |
| --- | --- | --- | --- | --- |
| R01 provenance/build | None | build/env scripts | UHT/UBT executed | Linux PASS; explicit UHT PASS; Win64 Turnkey PASS, compiler pending |
| R01 Windows SDK | None | compile/link/run smoke script | Real probe | FAIL host has no Wine; Podman denied |
| R01 Wwise | None | configuration verifier | XML/JSON/bank/library checks | PASS static; no audio runtime claim |
| R02 vanilla stations | Stations.lua | Saved external station config; no actor replacement | Native compilation | Runtime NOT RUN |
| R02 single resource | Programmable filter | Native per-slot allowed descriptor | UHT/UBT; policy tests | Runtime ingress NOT RUN |
| R02 resource changes | Stations.lua | Empty/undocked/unreserved validation | Native compilation | Runtime NOT RUN |
| R02 fluid station | Game APIs | Reads vanilla fluid class setting; rejects wrong descriptor form | Portable fluid compatibility cases | Pipe transfer/capacity NOT VERIFIED |
| R03 selector and labels | Programmable station UI | Server console config only | None | UI NOT IMPLEMENTED |
| R04 solid dispatch | Dispatcher.lua | Manual route adapter and pure offer policy | 67 policy checks | Automatic scheduler NOT IMPLEMENTED |
| R04 fluid dispatch | None established | Same medium-aware policy | Compatibility/full-load/retained cases | Native scheduler/quantities NOT VERIFIED |
| R04 retained priority | Dispatcher.lua | Pure policy only | Retained priority/partial cases | Native retained FSM NOT IMPLEMENTED |
| R04 exclusions | Registration/dispatcher | Policy checks; native driver/fuel checks | Policy driver/AP/fuel/busy cases | Full native eligibility NOT IMPLEMENTED |
| R05 nearest provider | Road path queries | Corrected portable ranking | Adversarial depot-distance cases | Native ranking/cache NOT IMPLEMENTED |
| R05 directed routes | Native Lua bridge | Three native directed reachability queries/readback | Native compile | Game acceptance NOT RUN |
| R06 reservations | Dispatcher.lua | Exclusive native job/endpoint primitives | 3245 core checks incl synthetic fleet | Runtime concurrency NOT RUN |
| R07 depots/fuel | Depot/RestStop modules | Vanilla-home prototype; no native rest-stop | Primitive ownership only | Native depot/refueling NOT IMPLEMENTED |
| R08 warehouses | [S] logic | Shared case-insensitive policy; manual pickup guard | Solid/fluid/unknown origin cases | Policy PASS; runtime NOT RUN |
| R09 Lua migration | Offline save tools | No importer; legacy functionality preserved | None | NOT IMPLEMENTED |
| R10 uninstall | Legacy dependent actors | Station restriction restoration with preflight; vanilla actors retained | Native compile | Full migration/mod-disabled load NOT VERIFIED |
| R11 management | Dashboard/Monitor | Native console status/list/config | None | Management UI NOT IMPLEMENTED |
| R12 persistence | Lua persistent records | SaveGame identities/config; conservative interrupted-job recovery | Pure reload state tests; UHT compile | Real save/load/restart NOT RUN |
| R13 package | Lua release archive | Alpakit CLI plus ZIP/PE validator | 4 negative archive tests; no release artifact | Win64 package NOT GENERATED |

## Continuation status

See [continuation](continuation.md) for implementation changes and current blockers.
The machine-readable [acceptance matrix](acceptance.json) now records implementation,
verification method and actual result individually for all 22 gates. Earlier table entries
about absent source implementations are historical; none imply a passed runtime gate.

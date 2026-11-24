# Native recovery continuation (source only)
Base: dd61a7a7c64ff891ad9af4e777441f113734f888. Source port of RecoveryFSM.lua and selected DepotRuntime.lua recovery behavior.
- Automatic travelling jobs with a native navigation error retain their phase and leases. A 5/10/20/40/60 second capped backoff limits route writes. Pending directed path calculation uses the existing bounded road cache.
- Retry uses the exact reserved route and existing target index, checks native road reachability from the current segment, and reads back route/target/AP. Fault clearance, not a route setter returning, permits normal observations to continue. No transfer is inferred.
- AP-off, driver-controlled, externally changed routes are not re-enabled or overwritten. Loading/unloading/docking, mixed or unexplained cargo, invalid target and unsupported state remain fail closed. Missing fuel waits without synthetic fuel.
- Runtime retry clock is intentionally not serialized; on reload a valid matching saved automatic phase is reconciled by the existing rebuild path, then retries start fresh.
- Scope: no alternate-anchor route fabrication, no off-road teleport, no automatic abandonment of reservations, no guessed cargo destination. The Lua home-ready cleanup and return replanning after incomplete delivery remain open.
- Native API declarations checked against SML v3.12.1. Source changes are not yet UHT/UBT or in-game verified. The portable tests are added to scripts/test-filn.sh; run them and the real native build locally. All 22 runtime/release gates remain open.
- No game process, save, installed mod, Steam setting or live player state touched.

# Continuation verification (2026-11-24)

[Current implementation and blockers](../../../specs/014-native-completion/continuation.md).
Authoritative snapshots, shared automatic dispatcher, native directed-road cache, transfer
observers, native parking hook, saved recovery, settings migration and server-validated station
controls are now implemented in source. Actual game and release acceptance remain unresolved.
Historical reports below are retained; use Spec Kit continuation evidence for current status.

# Current verification — native completion (2026-11-24)

See [current Spec Kit evidence](../../../specs/014-native-completion/validation.md)
and [traceability](../../../specs/014-native-completion/traceability.md). Linux
StarterProject and station-adapter UHT/C++/link have passed on installed SML 3.12.1.
Win64 Turnkey SDK detection now passes. Portable tests: 3245 core + 67 policy checks
on GCC/Clang. No completed Win64 package or runtime acceptance is claimed.
Historical entries below describe prior environments and are superseded by this report.

# Validation report — 2026-10-23

## Release preparation — 2026-11-24

- Native portable tests rerun: 3,245 checks passed.
- Full `scripts/test-filn-lua.sh` suite passed, including feature 013 registration
  and world readiness host regressions. No game APIs were exercised.
- Python suite: 93 tests ran, 85 passed; eight dashboard HTTP tests errored because
  the sandbox prohibits socket creation (`PermissionError: Operation not permitted`).
- Simplified Lua updater verified against a temporary game directory: all canonical
  modules copied, operator Config.lua preserved, original files backed up, obsolete
  settings modules removed. No installed game files were modified.
- Source handoff ZIP CRC and every manifest SHA-256 verified. Saves, runtime journals,
  logs and build caches excluded. This archive is not an SMM installation package.
- CSS engine absent at the configured default location. UHT/UBT, Alpakit Win64
  packaging and native runtime acceptance remain blocked/unrun. Lua dispatcher
  feature parity is absent from the native module. See `../../../RELEASE.md`.

## M0
Audited upstream commit `8eebc9e659`, SML 3.12.0, minimum game CL491125; header update
CL513885, engine association 5.6.1-CSS. Read actual headers and SML subsystem manager
implementation before writing integration code. Full API/provenance notes:
`../../../specs/001-filn-parking-routing/research.md`.
Native mod skeleton, canonical source, installation script and Spec Kit artifacts exist.

## Executed checks
- Spec Kit preset resolution, setup-plan.sh, setup-tasks.sh, check-prerequisites.sh:
  feature artifacts resolved successfully, including required task list.
- Initial red host test failed as expected: FILNCore.h did not exist.
- `bash scripts/test-filn.sh`: GCC C++20, -O2, warnings as errors, **3,245 checks passed**.
- `CXX=clang++ bash scripts/test-filn.sh`: same options, **3,245 checks passed**.
- Tested shared primitives: idempotent ownership, duplicate truck/bay, same station, busy truck,
  provider and requester rejection, no partial bookings after rejection, duplicate job, empty job,
  unknown release, release/reuse, valid/invalid journey transitions, reload fails closed.
- Synthetic reservations: 100/500/1,000 vehicles with 300 endpoint slots. Active count capped
  at 100/150/150. Final GCC batch attempt times 81/216/294 microseconds; Clang 103/271/406 microseconds.
  Single host samples, include string/allocation overhead. **Not scheduler or game timings**.
  No game-thread max frame time, engine autopilot cost, cache counts or memory bytes measured.
- Shell syntax checks passed for outer scripts/*.sh.
- Address/undefined sanitizer build attempted but failed at link: host is missing
  `/usr/lib64/libasan.so.8.0.0`. Sanitized executable was not produced or run.
- Descriptor/module name, SML-only plugin dependency, task format/status and absence of legacy
  route API calls checked successfully with Python assertions. These do not validate UHT/UBT.
- Installation copied plugin into StarterProject/Mods/FicsItLogisticsNetwork successfully.
- Upstream tracked diff check passed; original starter tracked files were not modified.

## Build blockers / NOT RUN
`bash scripts/build.sh` exited 1: `Custom CSS engine missing; see README.md.`
No toolchains directory, CSS UnrealEditor/UBT/UHT installation or Wwise integration exists here.
MSVC toolchain and Windows packaging prerequisites are not installed in this workspace.
The user's Distrobox doctor output confirms base dependencies exist there, but this sandbox
cannot enter it: Podman fails setting /run/user/1000/libpod permissions (read-only).
Shell GitHub DNS is unavailable; no engine download or account linking was attempted here.

Therefore: native adapter UHT parsing, compilation, linkage, module load, bootstrap/save
lifecycle, path query execution, route target reset, actual driving/docking, item transfers,
home stop/reassignment, save/load, multiplayer and Win64/Linux-server packaging are **unverified**.
No runtime log is presented as evidence. No cooked binaries or generated test map exist.

## M1 status and remaining work
Source prototype and explicit home fallback implemented; **M1 acceptance is not complete**.
Author custom ALTNParkingBay assets, compile/resolve UHT and linkage issues in the matching
engine, then execute docs/IN_GAME_TEST.md. Add an audited dock/undock event bridge before
fleet-scale use. Implement destroyed-actor cleanup/administrative recovery before production.
M2 automatic scheduler, M3 lazy cache, M4 telemetry and M5 FIN adapter remain deferred.

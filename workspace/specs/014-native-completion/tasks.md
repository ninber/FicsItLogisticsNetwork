# Native completion tasks

- [x] T001 Inspect baseline processes/log before launching any new compilation.
- [x] T002 Record installed StarterProject/SML/engine provenance and logged fresh build status.
- [x] T003 Diagnose original Turnkey failure separately from Windows SDK validation.
- [x] T004 Validate configured Wwise project XML, bank header/metadata and target libraries.
- [x] T005 Add container detection, NVIDIA creation opt-in, gh/XDG packages and compiler smoke probe.
- [ ] T006 Execute NVIDIA Vulkan probe in compatible container (session Podman runtime read-only).
- [ ] T007 Execute MSVC + Windows SDK compile/link/PE run in compatible container (host Wine absent).
- [x] T008 Pass UAT/Turnkey after repairing physical-path Alpakit C# build.
- [x] T009 Import pinned PR #1 policy and run GCC/Clang portable tests.
- [x] T010 Resolve nearest-provider conflict; test solid/fluid eligibility and storage rules.
- [x] T011 Implement saved station config, native slot restriction and safe change preconditions.
- [x] T012 Implement guarded restoration of previous restrictions on vanilla actors.
- [x] T013 Compile station adapter through UHT/C++/link on installed headers.
- [ ] T014 Verify slot filters against conveyor, pipe, manual and docking ingress in isolated game.
- [ ] T015 Implement native resource selector UI and scoped PROVIDER/REQUESTER switch labels.
- [ ] T016 Build authoritative snapshots, registered vehicle eligibility and exact fluid capacities.
- [ ] T017 Implement directed graph snapshot, bounded path computation/cache and invalidation.
- [ ] T018 Implement shared automatic scheduler with atomic reservations/fairness.
- [ ] T019 Integrate retained-cargo dispatch FSM and measured transfer completion.
- [ ] T020 Port native depot enrollment, actual refueling, home return and hold.
- [ ] T021 Reconcile saved jobs after restart/destruction/path changes without reservation leaks.
- [ ] T022 Implement legacy discovery/dry-run/journaled import, preserving all connections/state.
- [ ] T023 Implement complete disable/uninstall semantics and save without FILN acceptance.
- [ ] T024 Implement native management UI and validated multiplayer server commands.
- [ ] T025 Run Unreal integration suite and isolated solid/fluid/migration/recovery acceptance.
- [ ] T026 Execute headless cook/Alpakit Win64 packaging and inspect actual generated artifact.
- [ ] T027 Verify SMM installation/layout and independent mod-disabled disposable save load.
- [ ] T028 Update draft PR only after reviewable tested changes; keep draft until acceptance.

## Continuation: native adapter (source completion is separate from game acceptance)

- [x] T029 Add Distrobox argv-safe execution wrapper; route host SDK/package/doctor to existing environment.
- [ ] T030 Execute container discovery/Wine/SDK/package outside the restricted Podman runtime (blocked).
- [x] T031 Integrate authoritative native offer snapshots for enrolled stations/registered vehicles.
- [x] T032 Add retained-cargo endpoint reservations and preserve known non-warehouse cargo provenance.
- [x] T033 Add directed reverse-distance search with resumable edge/heap budget, native graph adapter, LRU and revision invalidation.
- [x] T034 Add per-requester/per-preset provider ordering and bounded automatic dispatch with native route/target/AP readback.
- [x] T035 Add source/target inventory transfer observers; reject unexplained inventory changes instead of inferring completion.
- [x] T036 Add saved automatic state/cargo counters and conservative matching-route reload reconciliation.
- [x] T037 Add native AP-tick parking hook and native supplied-fuel docking service; runtime verification remains open.
- [x] T038 Add server-validated resource/enrollment UI and original-label binding proxy with pooled-widget restoration.
- [x] T039 Add explicit vehicle enrollment and admin dispatcher/migration/preparation UI actions.
- [x] T040 Add version-1 idempotent legacy-filter settings import, preflight, rollback and unsupported-actor reporting.
- [ ] T041 Complete physical legacy actor/depot/vehicle-state conversion (runtime-generated legacy classes/fixtures needed).
- [ ] T042 Validate native parking hook, native transfer callback timing, fuel and fluid quantities in a disposable real game.
- [ ] T043 Complete full network management roster/pagination and visual/localization/multiplayer acceptance.
- [x] T044 Add real NativeInventory automation cases and fail-closed automation-report validation.
- [ ] T045 Execute/resolve native inventory and complete gameplay integration suite (generated FG stubs cannot establish native behavior).

- [x] T046 Add server-paginated station/provider/requester/vehicle/depot overview with actual cargo, fuel/AP/driver, reservations and diagnostics.
- [x] T047 Read back native station filter assignment/restoration and retain recovery metadata on failure.
- [x] T048 Revoke retained-cargo provenance after unexplained/manual/recovery inventory changes.

- [x] T049 Port Lua bounded-frequency native navigation retry policy; integrate guarded route/target reapplication for active automatic travel phases and preserve player-changed routes.
- [ ] T050 Build/UHT and verify native navigation retry, fault clearance, route readback, player takeover and indefinite bounded-frequency retry in disposable real game. Full alternate-route return, stalled-without-error recovery, and legacy migration remain unresolved.

- [x] T051 Port explicit Lua depot enrollment: reject ambiguous homes/trucks and unrelated routes, assign/read back home-only/AP-on, guard rollback and audit native fuel connections.
- [ ] T052 Verify real native home-only AP acceptance (including initially unfueled trucks), rollback, exclusive ownership, hold/fuel behavior, legacy manual pair workflow and explicit registration UI in disposable game.

- [x] T053 Add legacy recipe-based idle depot adoption and strict bounded Lua dispatch journal parser/route matching preview.
- [ ] T054 Complete active journal job restoration and physical class/recipe conversion; verify actual migration and save reload. Journal preview is not job migration.

- [x] T055 Implement native travel progress watchdog and bounded stall retries; restore home-only route before successful automatic completion and release reservations only after route/AP readback.
- [ ] T056 Verify actual native docking, hold, route restoration and stalled/repaired road behavior; alternate destination recovery and destroyed actor cleanup remain open.

- [x] T057 Implement explicit legacy journal import for unique delivery-only routes with matching cargo or empty home return; preserve native route/AP, acquire leases and count only future transfer events. Expose server console preview/import.
- [ ] T058 Extend migration to legacy pickup/alternate recovery routes without repeating pickups; verify imported jobs in actual save. Physical actor conversion remains open.

- [x] T059 Implement shared transactional station filter restoration and guarded uninstall preflight; rollback owned writes on failure and retain conflicting state.
- [x] T060 Implement bounded cleanup for positively observed destroyed vehicle identifiers; release only that registration's owned leases and repair indices.
- [ ] T061 Verify destruction lifecycle and independent uninstall/save load; unresolved references and surviving vehicles with missing endpoints remain fail-closed.

- [x] T062 Implement guarded abort-to-home routing using a distinct validated depot anchor and preserve cargo/leases until actual home arrival.
- [x] T063 Save stable reservation endpoint identities for new automatic and imported jobs; restore leases even when endpoint references are missing.
- [ ] T064 Verify native route writes, abort return, anchor behavior and save/reload in game. Recovery after native auto-sanitization of the old route and missing homes remains unresolved.

- [x] T065 Adopt legacy pickup-route suffixes after the provider, only with exact safe requester/home remainder and compatible observed cargo; refuse pre-pickup and ambiguous routes.
- [x] T066 Port full compatible native fuel readiness, pending stationary docking observation, measured fuel receipts and retry cooldown from DepotFuelService.lua.
- [ ] T067 Verify real-game native refueling/slot capacity, receipt timing and migration of in-flight pickup suffixes; legacy trips before pickup remain intentionally unresolved.

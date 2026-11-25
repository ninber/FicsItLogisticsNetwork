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
- **T022 — external/superseded:** Implement legacy discovery/dry-run/journaled import, preserving all connections/state. (Not a native release task.)
- [ ] T023 Implement complete disable/uninstall semantics and save without FILN acceptance.
- [ ] T024 Implement native management UI and validated multiplayer server commands.
- [ ] T025 Run Unreal integration suite and isolated solid/fluid/recovery acceptance.
- [x] T026 Execute headless cook/Alpakit Win64 packaging and inspect actual generated artifact. Evidence: local-verification.md.
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
- [x] T039 Add explicit vehicle enrollment and admin dispatcher/preparation UI actions.
- **T040 — external/superseded:** Add version-1 idempotent legacy-filter settings import, preflight, rollback and unsupported-actor reporting. (Not a native release task.)
- **T041 — external/superseded:** Complete physical legacy actor/depot/vehicle-state conversion (runtime-generated legacy classes/fixtures needed). (Not a native release task.)
- [ ] T042 Validate native parking hook, native transfer callback timing, fuel and fluid quantities in a disposable real game.
- [ ] T043 Complete full network management roster/pagination and visual/localization/multiplayer acceptance.
- [x] T044 Add real NativeInventory automation cases and fail-closed automation-report validation.
- [ ] T045 Execute/resolve native inventory and complete gameplay integration suite (generated FG stubs cannot establish native behavior).

- [x] T046 Add server-paginated station/provider/requester/vehicle/depot overview with actual cargo, fuel/AP/driver, reservations and diagnostics.
- [x] T047 Read back native station filter assignment/restoration and retain recovery metadata on failure.
- [x] T048 Revoke retained-cargo provenance after unexplained/manual/recovery inventory changes.

- [x] T049 Port Lua bounded-frequency native navigation retry policy; integrate guarded route/target reapplication for active automatic travel phases and preserve player-changed routes.
- [ ] T050 Build/UHT and verify native navigation retry, fault clearance, route readback, player takeover and indefinite bounded-frequency retry in disposable real game. Full alternate-route return, stalled-without-error recovery, remain unresolved.

- [x] T051 Port explicit Lua depot enrollment: reject ambiguous homes/trucks and unrelated routes, assign/read back home-only/AP-on, guard rollback and audit native fuel connections.
- [ ] T052 Verify real native home-only AP acceptance (including initially unfueled trucks), rollback, exclusive ownership, hold/fuel behavior, legacy manual pair workflow and explicit registration UI in disposable game.

- **T053 — external/superseded:** Add legacy recipe-based idle depot adoption and strict bounded Lua dispatch journal parser/route matching preview. (Not a native release task.)
- **T054 — external/superseded:** Complete active journal job restoration and physical class/recipe conversion; verify actual migration and save reload. Journal preview is not job migration. (Not a native release task.)

- [x] T055 Implement native travel progress watchdog and bounded stall retries; restore home-only route before successful automatic completion and release reservations only after route/AP readback.
- [ ] T056 Verify actual native docking, hold, route restoration and stalled/repaired road behavior; alternate destination recovery and destroyed actor cleanup remain open.

- **T057 — external/superseded:** Implement explicit legacy journal import for unique delivery-only routes with matching cargo or empty home return; preserve native route/AP, acquire leases and count only future transfer events. Expose server console preview/import. (Not a native release task.)
- **T058 — external/superseded:** Extend migration to legacy pickup/alternate recovery routes without repeating pickups; verify imported jobs in actual save. Physical actor conversion remains open. (Not a native release task.)

- [x] T059 Implement shared transactional station filter restoration and guarded uninstall preflight; rollback owned writes on failure and retain conflicting state.
- [x] T060 Implement bounded cleanup for positively observed destroyed vehicle identifiers; release only that registration's owned leases and repair indices.
- [ ] T061 Verify destruction lifecycle and independent uninstall/save load; unresolved references and surviving vehicles with missing endpoints remain fail-closed.

- [x] T062 Implement guarded abort-to-home routing using a distinct validated depot anchor and preserve cargo/leases until actual home arrival.
- [x] T063 Save stable reservation endpoint identities for new automatic and imported jobs; restore leases even when endpoint references are missing.
- [ ] T064 Verify native route writes, abort return, anchor behavior and save/reload in game. Recovery after native auto-sanitization of the old route and missing homes remains unresolved.

- **T065 — external/superseded:** Adopt legacy pickup-route suffixes after the provider, only with exact safe requester/home remainder and compatible observed cargo; refuse pre-pickup and ambiguous routes. (Not a native release task.)
- [x] T066 Port full compatible native fuel readiness, pending stationary docking observation, measured fuel receipts and retry cooldown from DepotFuelService.lua.
- [ ] T067 Verify real-game native refueling/slot capacity, receipt timing.

- **T068 — external/superseded:** Implement copy-only inverse legacy class/recipe/filter conversion, exact selection/hash guards, inverse body comparison, roundtrip check and native resource/depot manifest import. (Not a native release task.)
- **T069 — external/superseded:** Verify actual parser serialization and independent disposable load; resolve residual legacy schematic/component references and unsupported classes. Active depot conversion remains restricted to quiescent saves. (Not a native release task.)

- [x] T070 Port bounded idle home-only vehicle reconciliation: release stale holds, observe native return, retry exact empty home-only route with verified native roads, restore hold only at home.
- [ ] T071 Verify home-only native retry, actual docking and hold after moving/reloading an enrolled truck. Unmanaged or AP-off trucks are not automatically enrolled.

- [x] T072 Preserve localizable FText through station/overview RPCs, replace numeric FSM states with stable localization keys, and clamp stale overview pages.
- **T073 — external/superseded:** Expose converted station and legacy journal preview/import through existing administrator-validated management UI; fixed manifest filenames, no arbitrary client file paths. (Not a native release task.)
- [ ] T074 Verify localized RPC rendering in actual multiplayer and gather/compile translation catalogs; technical diagnostics remain raw English server log text.

- [x] T075 Discover ordinary LOAD providers from native station roster, rank directed compatible candidates, revalidate actual matching single cargo and exclude designated depots. Demand remains explicitly configured.
- [ ] T076 Verify ordinary solid/fluid provider dispatch, native filter behavior and station roster changes under real runtime load.

- [x] T077 Persist explicit native rest-stop designation independently of truck ownership; expose admin management, exclude them from cargo scheduling and restore vanilla behavior during uninstall.
- [ ] T078 Verify role markers, management access, saved unclaimed depots and actual fuel-only behavior in game. A distinct cooked build-menu recipe/descriptor is not implemented; vanilla actor designation is the current native alternative.

- [x] T079 Implement explicit parked rehome/recovery with exclusive ownership move, owned-lease release, home-only route readback, cargo preservation and AP-off preservation. Replace unsafe unrestricted reset path.
- [ ] T080 Verify operator-parked recovery to replacement depot after home destruction, save/reload and explicit AP opt-in. Fully automatic reassignment is intentionally not performed.

- [x] T081 Use signed bounded transfer-event ledger independent of delegate mutation timing, reconcile against authoritative inventory snapshots, and require route/AP/lease ownership at transfer and completion.
- [x] T082 Expose explicit parked recovery through admin-validated native UI RPC.
- [ ] T083 Verify actual native delegate order, duplicate/missing events, recovery RPC and save consistency in real game; portable accounting tests do not establish delegate behavior.

- **T084 — external/superseded:** Adopt exact pre-pickup legacy routes only for empty trucks and still-eligible full-load providers/empty requesters with native directed remaining legs, both endpoint leases, and no route/cargo writes. Permit explicitly designated converted depots in journal import. (Not a native release task.)
- **T085 — external/superseded:** Verify migration of pre-pickup and post-conversion journeys in real game, including native path semantics and duplicate prevention; reject ambiguous/docking routes. (Not a native release task.)

- **T086 — external/superseded:** Extend conversion/manifest import to 512 rows, support absent/empty programmable filters as ordinary vanilla stations without demand inference, and preserve the verified native force-fuel property. Add synthetic city-scale regression matching observed record counts. (Not a native release task.)
- **T087 — external/superseded:** Execute full parser roundtrip on a disposable real city copy; resolve exact per-actor filters, cargo, docking and residual schematic/recipe dependencies. Raw compressed-record inspection is not a full object parser. (Not a native release task.)

- **T088 — external/superseded:** Decode actual unlock arrays; implement opt-in exact legacy unlock cleanup on offline copies with inverse comparison, fail closed on remaining actor/asset use, report other dependencies. (Not a native release task.)
- **T089 — external/superseded:** Verify parser roundtrip and resolve the remaining historical construction-statistics class key before independent legacy-disabled load. Do not discard statistics or unrelated mod data. (Not a native release task.)

- **T090 — external/superseded:** Share typed residual dependency scanning between conversion and unlock cleanup; preserve BigInt/Map statistics without JSON serialization failure. (Not a native release task.)
- **T091 — external/superseded:** Establish explicit statistics migration semantics for CurrentMax and typed parser representation; preserve history and block dependency-free claims until then. (Not a native release task.)

- [x] T092 Remove embedded legacy importers, saved import metadata, legacy recipe detection, migration console/RPC/UI actions and importer policy tests. Keep general native enrollment, recovery and uninstall. External save tools are not shipped in the mod.
- [x] T093 Verify the standalone native package has no legacy import or save-specific dependency; rerun current-head tests/build. Static source/package/import inspection and both portable compilers/native targets passed; runtime acceptance stays open.

- [x] T094 Remove separate AP-enabling/manual unmeasured dispatch path; use common native offer/commit path. Verify live slot filters and medium, protect reserved route writes against reentrant state change, compare equal nearest-provider distances by empty leg, unify parked radius/vertical/foreign-dock checks.
- [ ] T095 Verify actual native dispatch, road ties, live filter changes and route setter reentrancy in runtime; portable checks do not prove game API behavior.

- [x] T096 Preflight native saved registry identities, endpoint leases, states and configuration before native route/filter reconciliation; reject duplicate IDs, live mismatches, and unknown phases; preserve unresolved endpoint IDs and revoke inconsistent idle provenance.
- [ ] T097 Verify current-head native save/load with valid and conflicting own-state fixtures, missing endpoints and AP-on/off vehicles; no legacy save migration involved.

- [x] T098 Capture authoritative identifier OnDestroyed events before GC, defer reclamation until both observed vehicle owners are gone, remove observer/index by stable truck identity, and defer own-state rebuild after a late PostLoadGame callback.
- [ ] T099 Verify native actor destruction/streaming/GC and load callback ordering; unresolved pointers without explicit destruction evidence remain retained.

## Consolidated standalone source pass
- [x] T100 Rebind idle native inventory observers by stable truck identity; reject stale callbacks and stop active jobs if their observed inventory changes.
- [x] T101 Use one checked filter transaction for new configuration, resource changes, own-state reload and removal; distinguish missing inventory from unrestricted slot.
- [x] T102 Explicit uninstall stops owned parked AP-on vehicles without altering routes, disables dispatch, restores filters and clears native saved registry; empty prepared subsystem declines saving.
- [x] T103 Revalidate active resource/role/storage constraints and prevent an invalid endpoint route from continuing while asynchronous recovery is pending; use native directed queries to a safe unclaimed depot anchor or fail closed.
- [x] T104 Discover all portable test files and run GCC/Clang regression CI; do not claim native compilation from portable CI.
- [ ] T105 Verify consolidated source on current SDK and runtime, including observer replacement, filter rollback, AP-off uninstall, safe abort and save behavior.

- [x] T106 Make normal station interaction expose a compact FILN registration entry; add administrator parked unregistration, job/road diagnostics and avoid graph ranking for irrelevant cargo. Preserve ordinary switch labels.
- [x] T107 Remove retired canonical importer sources from generated StarterProject copies on installation; update the current native README instead of retaining contradictory prototype instructions.
- [ ] T108 Verify actual UI attachment/layout, unregistration, provider ranking performance and the refreshed build copy with current Unreal SDK.

- [x] T109 Port the proved Lua home-only TooFewStations eligibility and five-second activation grace, distinguish initial retained-cargo home docking from premature return, reconcile same-identifier actor/inventory replacement against measured cargo, bridge missed departure observations only with matching reserved transfer events, and hold final physical home arrival pending watchdog reconciliation.
- [ ] T110 Verify native AP state transition, proxy/significance actor changes, short docking intervals and final home hold under actual gameplay. No portable test establishes engine hook semantics.

- [x] T111 Address reported missing native interaction controls: resolve actual interaction object (station, identifier, component, hit wrapper); attach to existing live UMG panel rather than replace root; retry on Native_Init; clear pooled context on removal/destruct; defer descriptor/vehicle enumeration until panel opens. Add bounded interaction diagnostics.
- [ ] T112 Reconcile with local verification commit b4bfb3e (not pushed), compile/package and manually inspect real station UI in a disposable native-only session. Missing UI is an observed failure; exact original cause remains unconfirmed. External migration stays outside plugin. No continuous polling or test harness redeployment.

## Player workflow correction (2026-11-25; supersedes UI tasks T015/T024/T038/T039/T043/T046 and manual enrollment T051/T052)

- [x] Remove the injected station management window, RCO controls, and LOAD/UNLOAD relabeling from source.
- [ ] Verify original programmable FILTERS selection through native dispatch with required Smarter Truck Stations dependency and no Lua. Solid gameplay verification remains missing; standalone selector and fluid support deferred by user.
- [ ] Create and cook a distinct native depot recipe/building; automatic discovery of newly built depots is blocked on this asset. Existing recorded native depots are not a substitute.
- [ ] Verify automatic adoption of AP-on depot-only routes, including empty and retained cargo, no manual registration, save/reload and ordinary vehicle exclusion.
- [ ] Verify automatic overflow relocation of the new truck, never the old owner, through native directed routes, exclusive ownership, physical arrival, route collapse, no-free-depot and failed-route cases. Source prototype only; no runtime acceptance.
- [ ] Port existing dashboard telemetry to native C++; no in-game management UI.
- [ ] Run build/UHT/cook and all real-game acceptance, with no external test harness, no Lua dispatcher, no automated game UI.

- [x] Declare versioned TruckStationSettings dependency and guard new solid-only enrollment/configuration/dispatch; retain fluid-aware portable policies for future support.
- [ ] Verify installed dependency, solid requester FILTERS and actual delivery; verify fluid trucks/stations are not enrolled or dispatched. No standalone selector required for current scope.

- [x] Audit installed Smarter Truck Stations source and SML 3.12.1 target-inventory delegate contract; implement independent after-operation unload receipt accounting, no replacement cargo writes.
- [ ] Verify programmable UNLOAD native hook installation/thread context, full/partial returned quantity, no duplicate delegate credit, real recipient inventory and route recovery. See smarter-truck-stations-integration.md. If callback is off game thread, fail acceptance and implement a safe receipt bridge.

- [x] Add exact native depot recipe identity, bounded discovery, SML Initialization-phase recipe registration and unlock following vanilla truck station availability; provide editor asset authoring script.
- [ ] Execute editor authoring script, inspect generated .uasset files, cook, verify actual Build Gun placement, docking, save/reload and independent uninstall. Source only; not done.
- [x] Remove retired menu translation unit from generated StarterProject installs; keep user-authored Content.

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

- [x] Implement native demand-gated snapshot export and existing browser dashboard native read-only mode; add HTTP/model tests.
- [ ] Verify real native dashboard snapshots, correct path, no frame-time regression, capacity/route-evidence parity and save/world teardown; no Lua dependency.

- [x] Replace game-thread-only unloading receipt assumption with bounded job-scoped native station operation aggregation for LOAD and UNLOAD; collect worker results without dispatch mutations, verify exact cargo ledger on game thread.
- [ ] Verify actual callback scheduling, concurrent native transfer and registry teardown, delegate/no delegate, partial/full transfers, owner replacement, user takeover, mismatch retry, completion and real save/reload. No source/portable-only success claim.

- [x] Guard dependency/source, native depot assets and cooked content presence before compilation/package; author-depot commandlet requires success receipt; test missing/LFS/incompatible dependency and source-only package cases.
- [ ] Execute actual author-depot + native compile + package and inspect actual Unreal pak entries. Asset headers/container presence are not proof of content correctness.

- [x] Fix simultaneous home-only claim starvation and avoid full roster scans per alternative depot candidate; revalidate route/node index immediately before mutation. Confirm Build Gun recipe listing through native manager readback.
- [x] Implement bounded actual per-resource capacity sampling for representative vehicle class/slot types; preserve explicit unknown quantities and exclude fluid vehicles from native dashboard eligibility.
- [ ] Verify multiple simultaneous AP-on home assignments, foreign multi-stop routes, large fleet timing and capacity parity in isolated game.

## Standalone solid/fluid station correction — latest user direction (2026-11-25)

Supersedes required TruckStationSettings dependency, external FILTERS adapter, fluid deferral/exclusion and Smarter Truck Stations-specific acceptance. Prior commits remain historical evidence; no source completion claimed by this spec update. Reconcile local Codex unity-build and depot-authoring fixes before changing shared native source.

- [ ] Remove TruckStationSettings plugin dependency, reflected external class/filter discovery, package/build prerequisite, and any runtime/cooked hard/soft references. No optional compatibility mode. Preserve original Lua experiment as historical tooling, not native runtime.
- [ ] Implement own server-authoritative per-station resource selection with explicit medium compatibility, persisted selection/readback and empty/undocked/unreserved transaction; retain normal station actor and physical connections. Handle fluid pipe ingress and units explicitly; no cargo mutation from UI.
- [ ] Author original native-looking selector assets using base-game docking station widget, native descriptor list/tab components and supported SML widget extension. No global generic panel, relabeling or manual register/apply controls. Verify solid and fluid widget paths independently in fresh editor/game sessions.
- [ ] Use native public station resource-form and vehicle docking-resource APIs; verify liquid/gas forms against real SDK/game; retain actual native docking result receipt bridge, test full/partial solid and fluid ingress/docking and fail closed on mismatch. Replace temporary solid-only enrollment and dispatch guards only after applicable adapters and docking checks exist.
- [ ] Reconcile native depot authoring failure using native SML root module discovery (one root per type); explicitly cook/validate assets in clean editor process. Verify shared depot compatibility or provide medium-specific depot if required by actual docking behavior.
- [ ] Verify standalone build/Win64 package and isolated solid + fluid deliveries with TruckStationSettings, Lua dispatcher and UE4SS absent. Migrate only external copies of legacy saves; do not install or touch original City/running game.

- [x] Reconcile top-level, workspace, native README, release and traceability instructions with standalone solid/fluid scope; package verifier rejects required/optional/disabled third-party dependencies.
- [ ] Resolve source inconsistencies enumerated in consistency-audit.md; no runtime/build readiness claim. Reconcile local Codex branch first.

- [x] Remove native third-party manifest dependency, reflected programmable-class/filter reader, and external development prerequisites; reject non-SML metadata; old saved unowned-filter marker only blocks eligibility. Static source regression and standalone prerequisite tests added.
- [ ] Integrate local unity-build fixes, compile standalone with third-party absent, inspect actual cooked assets/imports and verify game. Standalone selector and fluid support remain required and incomplete.

## Fluid station implementation pass (2026-11-25)

- [x] Replace generated root Blueprint with native SML root; author solid depot, fluid depot, fluid logistics station descriptor/recipe pairs from vanilla parents (editor resolves actual recipe path and validates product/medium). Require independent fresh-process reload; reject retired root Blueprint without deleting it.
- [x] Replace temporary solid-only gates with actual native station/vehicle resource form compatibility in configuration, pairing/overflow, offer construction, active endpoint checks, parking, fuel and recovery. Preserve one resource, exact native capacity, empty requester, transfer receipts and reservations.
- [x] Export medium-aware native telemetry and capacity samples; never label raw fluid quantities as liters or cubic meters without measurement. Add portable compatibility and dashboard regression cases.
- [ ] Reconcile local unity-build fixes and retired root asset; run matched Unreal editor compilation, actual authoring/reload, inspect cooked asset paths and place all three buildings in isolated game. Confirm native root discovery/registration and parent unlock behavior.
- [ ] Verify actual fluid station/vehicle forms, native docking, pipe ingress and descriptor lock, per-slot capacity, returned transfer amount vs cargo delta, actual fuel and same-medium overflow, save/reload and performance. Verify gas independently if permitted by base game.
- [ ] Author and verify own native-looking single-resource selector for both station types; currently only existing developer console configuration backend is available. No normal player setup claim.

## P0 — Depot-driven membership correction (2026-11-26, R07/R07a/R07b)

The latest player requirements supersede older home-only enrollment assumptions (including T051/T052/T070/T071 and older acceptance descriptions). Existing checked boxes are historical implementation records, not acceptance of the corrected behavior. These are open implementation and game-verification gates; do not mark them complete based on portable tests alone.

- [ ] T116 Separate durable vehicle membership from home ownership, parking, AP/driver, fuel, power and dispatch readiness. Discover every vehicle whose route contains a native FILN depot at any index and every actual new arrival at a native FILN depot; do not restrict discovery to home-only routes. Preserve ordinary non-depot routes, including two-stop coal supply. Detect ambiguous depot references, log exact blockers, maintain exclusive home assignments and avoid capturing player-driven/AP-off vehicles for motion.
- [ ] T117 Safely adopt loaded and empty multi-stop depot routes in every world, not just migrated City. Preserve actual cargo/fuel and native path data. Stop legacy repeat circulation, return to the assigned/available compatible depot with verified native routing, and establish home-only parked intent only after verified arrival. Prevent old in-flight routes from competing with new reservations during transition. Preserve existing owner when choosing an overflow depot; retain pending membership if no reachable bay exists.
- [ ] T118 Reconcile explicit player route edits. Removing all FILN depot nodes unregisters the vehicle and safely releases only its owned leases, home claim and fuel/parking hooks, including while active or awaiting fuel. Preserve player route, AP/driver settings, cargo and fuel. Do not mistake FILN-generated route changes, transient native state or an unchanged docking observation for player opt-in/out. Verify re-enrollment on a new explicit depot route or actual later arrival. Handle save/reload, actor replacement and map/index consistency.
- [ ] T119 Recover trucks stopped slightly short of their home by premature legacy refuel. Separate approach/parking, confirmed stopped home arrival, hold and fuel service. Use verified native movement; never force docking remotely or start fuel service before verified arrival. Report measured parking offset and individual power/standby/occupancy/path blockers. Confirm parked vehicles stay parked until eligible work.
- [ ] T120 Fix full-tank fuel-type mismatch deadlock: separate native refuel compatibility from safe departure readiness; use existing valid fuel without discarding or synthesizing fuel. Preserve full compatible fueling policy where applicable; do not assume any partial fuel amount is sufficient for a journey. Power/fuel problems do not remove membership.
- [ ] T121 Implement and validate priority for eligible vehicles already carrying the requested resource before empty vehicles. All deliveries, including full and partial retained cargo, MUST use depot → provider → requester → depot. No direct-delivery branch or omitted provider. Replace the existing provider-less retained-cargo offer, endpoint reservation and FSM behavior; retain provider selection rules, reserve both endpoints and validate native route readback. Account for initial cargo, top-up capacity and actual transfer receipts; a full vehicle visiting a provider legitimately loads zero additional items. Preserve residual cargo at home and prioritize it again. Keep compatibility, capacity and existing [S] constraints; report blockers explicitly.
- [ ] T122 Add automated regressions for enrollment/unregistration, loaded/empty old routes, multiple depots, AP-off/player control, unpowered or occupied bays, near-home parking, fuel mismatch, retained-before-empty vehicle priority with mandatory full routes for empty, partially loaded and fully loaded trucks, zero additional loading at provider for a full truck, save/reload, and conflicting legacy routes. Tests must fail until the corresponding runtime implementation exists; no simulated acceptance of missing behavior.
- [ ] T123 Verify a single integrated cooked native build in a disposable copy of City and a small fresh-world fixture. Record each depot-route vehicle's membership, readiness reason and route; confirm no endless legacy circulation, ordinary non-FILN routes unchanged, real parking and refuel, matched residual-cargo deliveries on full provider routes and player opt-out with no cargo/fuel loss. Capture durable transfer receipts, per-vehicle outcomes and explicit unresolved failures. Keep external Lua dispatcher/test harness disabled. Do not modify the original save or interrupt a running game. Overall migration acceptance remains FAIL until these conditions and station filter/inventory acceptance pass.

## P0 — In-game Organizer (R11, 2026-11-26)
- [ ] T124 Native interaction window and shortcut, close/Esc, focus, no overlapping menus; standard in-game key rebinding (temporary configurable key is not final acceptance).
- [ ] T125 Shared complete snapshot, open-only demand, no extra world scan or stale disk/world data, accessible when blocked; verify latency and bounded work.
- [ ] T126 Vehicles default: all actual cargo including parked/mixed, complete actual route/current target, names and state, include unenrolled trucks.
- [ ] T127 Stations: by descriptor, provider/requester columns, actual stock/capacity, saved resource even if filter invalid, no depots or inferred requester resource; verify fluid units.
- [ ] T128 Diagnostics last: exact registry, station and vehicle errors without weakening validation.
- [ ] T129 Visual acceptance: game names/icons, cohesive compact game-compatible style, 1080p/4K screenshots, long routes, scrolling, keyboard and mouse/focus. Exactly three tabs; no controls that mutate logistics.
- [ ] T130 Cooked host/single-player and remote multiplayer read-only transport, world transitions, no background scan when closed, shared web data. Remote access explicitly unavailable before authorized transport is implemented.

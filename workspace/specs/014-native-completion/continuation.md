> Publication checkpoint: the user subsequently authorized committing and pushing this
> incomplete development snapshot to feature/native-dispatch-policy. Earlier no-push
> statements below describe the implementation checkpoint. Verification status remains
> unchanged: all 22 runtime/release gates are unresolved, and this is not a release.

# Native continuation: implementation, evidence and limits

Existing verification history is retained in validation.md. No game save, installed mod,
Steam setting, active player, game process or input was changed. No push or publication.
Canonical source remains workspace/mods/FicsItLogisticsNetwork; the local review checkout
remains feature/native-dispatch-policy. Initial local changes were preserved.

## What changed

- `scripts/dev-exec.sh` selects an existing running development container by executed
clang/Wine/Vulkan probes and forwards argv safely through Distrobox. `headless.sh package`
and `sdk`, and doctor, delegate when this shell has no Wine. No Wine reinstall, container
creation, toolchain download or cache deletion. `FILN_DOCTOR_LOCAL=1` explicitly probes
only this shell. Native Linux compilation remains usable directly where it already works.
- `LogisticsDispatch.cpp` samples actual native inventory, power, standby, driver, AP,
fuel, capacity and saved enrollment. Both media use the same policy and dispatcher.
Retained candidates are evaluated first. Road ordering is shared by requester/preset;
for each vehicle only the first eligible provider is needed. Reservation precedes route
write, followed by route/target-index/AP readback; no optimistic success after failure.
- `FILNRouting.h` and `FILNRoadCache.h` use directed native SegmentLength and
CanVehicleTraverseSegment, not straight-line coordinates or waypoint counts. Reverse
Dijkstra is resumable, including high-degree vertices. Cache holds 32 entries, rejects
networks above 8192 nodes/32768 segments, and keys actual network/preset/target plus
network identity/revision/counts. Inventory and job changes do not clear distances.
Native graph-generation correctness after all types of topology edits remains unverified.
- `FILNTransferObserver.cpp` subscribes to multicast cargo inventory events without
replacing native delegates. Credit requires the actual provider source/requester target
inventory and native docked vehicle. Snapshot disagreement enters recovery; suppressed
callbacks are not silently replaced with inferred delivery success. Actual callback order
and native docking ingress require game verification.
- Saved automatic kind/resource/expected/loaded/delivered/remaining counters and cargo
origin flags support matching-route recovery. Unknown/mismatched state remains stopped
and reserved. Partial matching cargo can return home; known factory-origin retained cargo
may serve warehouses, unknown or warehouse-origin retained cargo may not.
- `FILNParking.cpp` uses the native ShouldTickAutopilotComponent hook to hold only owned
idle trucks, retaining a valid native route and AP on. Job reservation releases the hold.
No per-frame world scan or position poll. Player takeover/route changes relinquish it.
The game-thread calling assumption and physical hold/release still need actual runtime
acceptance; editor stub implementations cannot validate this hook.
- `LogisticsFuel.cpp` makes bounded native DockActor requests for eligible idle registered
vehicles with player-supplied station fuel. The game performs fuel transfer and burning.
No inventory insertion or synthetic fuel. Home stations reject connected cargo belts/pipes;
the vanilla home fallback still has physical unused cargo ports and is not a new rest-stop
recipe. No range/fuel-energy sufficiency forecast has been proven.
- `FILNStationControls` adds native resource selection, registration/removal, nearby-vehicle
enrollment, dispatcher control, migration preview/import, uninstall preparation and server
status. RPCs validate world, authority, proximity, rate, resource compatibility/availability,
station restrictions and admin access for global actions. A transient replicated component
marks enrolled stations. Ordinary station interaction is left intact. Explicit `filn.manage`
opens registration controls; the vanilla station extension only attaches to marked stations.
LOAD/UNLOAD text bindings are proxied, retaining original widgets/style/switch behavior;
pooled widgets restore original bindings. Rich-text/custom Blueprint label implementations
and component replication are not validated visually or in multiplayer.
- `LogisticsMigration.cpp` imports a single unambiguous FilterItem array, preserving names,
actors, transforms, connections, native modes, inventory/fuel and routes. Batches preflight,
restore newly changed filters on failure, lock dispatch rather than overwrite rollback
conflicts, retain a saved report/version, and avoid duplicate configs on repeat. This is
settings import only: legacy actor classes keep their original dependency. Unsupported
legacy depots/actors are reported, never replaced or deleted. Depot enrollment and persistent
Lua job/vehicle journals are not yet imported.
- Remove/preparation restores prior filters and removes transient station markers; native
physical actors/cargo/fuel/connections/routes remain. Full mod-disabled save loading remains
unverified. Disable retains FILN and holds idle vehicles; preparation releases FILN ownership.

## Exact packaging blocker

Commands executed from the restricted Bazzite host shell:

```
distrobox list
podman ps -a
bash scripts/headless.sh package
```

Each fails before a container is entered: Podman cannot chmod /run/user/1000/libpod on the
read-only filesystem. `headless.sh package` exit 1 and full output are recorded in
`logs/continue-native/package-environment.log` and `.exit`. Wine has NOT been declared
missing from the machine. Existing on-disk container metadata contains both
`satisfactory-modding` and `satisfactory-modding-nvidia`, both with NVIDIA device mounts.
A functional per-container probe could not be executed here, so neither name was assumed.
This session's permission policy automatically disallows sandbox escalation; there is no
available outside-sandbox approval path to request. No permission was fabricated.

## Minimal user action for the environment blocker

From an ordinary host terminal (game can remain running):

```
cd /var/home/ninber/migrationPlan/Dev-folders/D-Dev/SatisfactoryModding
bash scripts/dev-exec.sh bash scripts/doctor.sh
bash scripts/dev-exec.sh bash scripts/headless.sh prepare-uat
bash scripts/dev-exec.sh bash scripts/headless.sh package
```

Expected: the wrapper names a container only after successful compiler/Wine/NVIDIA Vulkan
probes. Doctor must pass the real SDK compile/link/PE run. Alpakit must finish successfully
and create an archive; inspect it with `python3 scripts/verify-native-package.py ARCHIVE`.
These commands do not deploy, install or launch the game. They are required only because
Codex cannot write the host Podman runtime from this session. No recreation is requested.

## Remaining technical limits

The generated StarterProject FGInventoryComponent.cpp contains empty Resize and default
IsItemAllowed implementations. Editor linkage/UHT passes do not establish game behavior.
Real inventory tests intentionally keep those failures. Full integration needs a disposable
matching real game with legacy fixture objects, which is not available without separate
runtime preparation. No action against the active save is authorized.

Physical class conversion needs legacy generated-class data plus native replacement and
save/reload connection preservation evidence. No class redirect or replacement is claimed
safe merely from a header declaration. Full depot/job import, automatic interrupted-path
replanning, all visual/game/multiplayer/save/removal acceptance
remain open. All 22 runtime/release gates remain unresolved until actual evidence exists.

## Executed continuation checks

- First automatic adapter build: logs/continue-native/automatic-build.log/.exit, exit 0.
- Final functional source plus provenance: provenance-build.log/.exit, exit 0, 50.63 s.
- Integration-fixture fix rebuild: integration-fixture-build.log/.exit, exit 0, 42.79 s.
  Both UHT and native compilation/link executed; this remains a Linux editor module.
- GCC and Clang portable suites: final-portable-gcc.log and final-portable-clang.log,
  3251 core + 70 policy + 1341 directed routing checks passed on each compiler.
  New checks cover retained reservations, known-origin warehouse rules, directed paths,
  unreachable nodes, invalid edges and high-degree work bounds. Not game-thread timings.
- Four negative package-verifier tests passed; no package artifact substituted for success.
- Headless integration first exited 139 before tests: default DDC/Zen writable paths are
  outside this sandbox. Retry used the supported DDC-ForceMemoryCache fallback without
  deleting existing disk caches. Initial fixture retry exposed a duplicate world initialization
  (CreateWorld already initializes it); removed the duplicate call, keeping assertions.
- Final headless integration: `FILN_INTEGRATION_MEMORY_DDC=1 timeout 90 bash scripts/headless.sh integration`,
  actual exit 1. Both FILN.NativeInventory.Fluid and .Solid ran and FAILED: Resize(2)
  yielded 0 slots in generated FactoryGame code. Exact log: integration-fixed-fixture.log.
  Machine evidence: logs/native-integration.AS1DCr/index.json (0 succeeded, 2 failed).
  The report verifier understands Unreal's UTF-8 BOM and rejects the failed cases.
- Headless startup also logged the missing Linux Wwise bank platform (the project and
  generated ProjectInfo currently list Windows only), and sandbox socket failures. Windows
  banks/libraries remain as previously verified. Linux bank generation through installed
  Authoring still requires the inaccessible Wine/container environment; no dummy banks copied.
- Final packaging/doctor routing: package-final.log/.exit and doctor-environment.log/.exit,
  exit 1 before Distrobox entry due to read-only Podman runtime. No new Win64 archive.
- The ordinary host metadata lists NVIDIA devices in BOTH development containers, so
  names/device mounts alone do not establish which one passes functional Vulkan/Wine probes.
- Spec Kit prerequisites with required tasks passed. All 22 actual runtime/release gates
  remain unresolved; the native inventory gate now has executed failing integration evidence.

All intermediate compile/UHT failures were retained (RPC replication macro parameter name,
TObjectPtr iteration, widget construction signature, delegate reflection/const binding), then
corrected and rebuilt. No failing acceptance assertion was removed or weakened.

## Final source hardening

Native filter application and restoration now read back every slot. A void setter cannot
produce a metadata-only success; assignment failure rolls back prior restrictions, and
restoration conflicts retain saved metadata and lock dispatch. Uninstall markers are only
removed after all restoration checks pass. Unexpected/recovery/manual cargo changes revoke
origin eligibility before any future warehouse retained-cargo assignment.

The management UI now includes a server-paginated overview (32 rows per RPC) of enrolled
stations, providers/requesters, solid/fluid resource identity and cargo, reservations,
registered vehicles/home depots, active states, AP/driver/fuel/hold flags, transfers and
diagnostics. It is a real native widget/RPC implementation, not an external dashboard.
Visual style/layout, remote replication and localization still require real game acceptance.
Empty fluid station/truck compatibility uses the native reflected class setting rather
than learning transport medium from currently absent cargo.

Readback and recovery safety builds passed in 50.16 s and 42.41 s respectively. The
management overview build passed UHT/native compile/link in 49.83 s. Final source includes
explicit inventory/slot headers to avoid depending on unity include order.

- Latest native source build: logs/continue-native/final-management-build.log/.exit,
  actual exit 0; FILN compile/link succeeded in 43.11 s. No subsequent C++ edits.
- Explicit UHT run: uht-final.log/.exit, exit 0, 6.85 s (automatic UHT also passed
  for the later management RPC header changes during the successful native build).
- Shared functional probe now requires an enumerated deviceName=NVIDIA, not merely an
  NVIDIA loader-error string next to another GPU. Three CLI-classification unit tests
  passed; those synthetic command-output cases do not verify real hardware or Wine.
- Final package command still exits 1 at Podman; no Win64 DLL/archive or package passed
  artifact inspection. setup scripts were edited only; no installation/recreation run.
- Source changes synchronized to the existing local feature/native-dispatch-policy review
  checkout, preserving unrelated Build.cs whitespace and existing StarterProject config.
  No commits, pushes, PR changes, live mod deployment or game/save actions performed.

Decisions made without further guidance: automatic dispatch defaults disabled until explicit
opt-in; current valid native route is retained while the native tick hook holds an owned idle
truck; marker components are transient and rebuilt from saved external configuration; resource
compatibility uses native class flags for empty inventories; legacy conversion stops at a
transactional settings import until class/connection/save preservation can be proven. These
choices do not satisfy the outstanding real-game acceptance gates by themselves.


## Current GitHub verification

See [local-verification.md](local-verification.md) for current-main Linux/Win64
builds, passing host suites, preserved native development archive and the two
executed failing SDK inventory tests. Historical blockers above are retained.

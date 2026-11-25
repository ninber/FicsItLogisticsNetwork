# Project consistency audit — 2026-11-25

Audited remote review branch b8c03a04a5ca1cd7acfe5933cd2d329a70bdd4f2; main 838a7f24. Local Codex reports native FILN and dependency compiled/linked after unity-build collision fixes, portable GCC/Clang and 123 Python tests passed, but Blueprint root-module authoring and fresh asset reload failed. Local fixes/logs not on remote; no independent verification or source overwrite. No cooked assets in repository. This audit reconciles guidance and package dependency contract, not native implementation.

| Location | Finding / required correction |
|---|---|
| FicsItLogisticsNetwork.uplugin | Still requires TruckStationSettings; remove entirely with own selector. |
| LogisticsPlayerWorkflow.cpp | Exact third-party class path, reflected FilterItem, ExternalFilter config bypass; remove adapter, use persisted FILN config. Preserve active leases; do not silently convert external records. |
| LogisticsSubsystem.cpp / LogisticsStationRegistration.cpp | ConfigureStation temporary solid-only guard precedes older medium logic; ExternalFilter skips restoration and native config. Replace with own checked medium transaction and explicit clear, authoritative UI, reject incompatible/nonempty/docked/reserved edits. |
| LogisticsDispatch.cpp | Solid-only gates in BuildOffer and Schedule; medium-aware policy remains. Use actual resource form, quantities, pipe ingress and docking compatibility, not simply delete guards. |
| LogisticsPlayerWorkflow.cpp / Pair / LogisticsFuel.cpp | Solid-only enrollment and fuel; verify compatible vehicle/depot docking before expanding. |
| FILNDepotModule / LogisticsDepotContent / author-native-depot.py | Blueprint root discovery fails locally; compile-before-CDO flag/asset registry likely cause. SML discovers native roots directly. Reconcile local fix; exactly one native root, explicit cook dependency, independent reload. |
| FILNDockReceipts.cpp | Generic native result bridge useful for both media; worker thread/direct getters, exact cargo units, receipt ordering and lifecycle unverified. No replacement unload unless native behavior demands it. |
| LogisticsTelemetry.cpp / filn_dashboard_model.py | Solid-only selection/capacity and 'fluid support deferred'; revise medium/units, compatible vehicles and route evidence, preserve read-only bounded sampling. |
| scripts/check-native-prerequisites.py + tests / build.sh | Still requires TruckStationSettings source/assets. Remove after own code/assets ready, clean standalone compile. Existing tests verify obsolete prerequisites. |
| scripts/verify-native-package.py + tests | Corrected SML-only metadata, rejects third-party even optional/disabled. Old draft deliberately fails release verification. Actual cooked imports/content still need inspection. |
| scripts/save-tools/native-station-conversion.cjs | External conversion exports filter and strips legacy class/recipe, not complete enrollment. No conversion back to third-party. Apply own resource and depot recipe after native proof; preserve originals. |
| Native UI assets | No independent selector or authored depot assets in tree. Use original game components, no unlicensed copies. |
| Tests / CI | Portable CI baseline 6aa490ee passed; no runtime proof. Earlier generated SDK Resize tests failed; retain. Add solid/fluid selector/ingress/receipt/docking, absent dependency, cook/reload, profiling, multiplayer. |
| Uninstall | Vanilla actor identity not enough: mod recipe references require safe conversion/dismantling and independent mod-absent load. |

Current authority: spec.md, standalone-station-implementation.md, plan.md, acceptance.json and latest tasks section. Historical Lua experiment and features 001–013 remain references, not release alternatives.

Continuation: reconcile unpublished fixes/logs; solve depot native-root and asset reload, own solid/fluid selector; remove third-party dependencies and prerequisites, enable verified medium-aware configuration, docking, enrollment, fuel, dispatch and telemetry; actual SDK and gameplay tests. Never touch original City or running game. No generic management menu, no external harness.

Native runtime and development prerequisite code intentionally unchanged until coordinated with local fixes. Package metadata enforcement exposes the inconsistency rather than accepting superseded artifacts.

Active native architecture, headless and gameplay acceptance instructions have also been updated; historical UI, depot, dashboard and integration research has explicit scope/evidence notices.

## Follow-up d49e4b4 — dependency removed in source

The earlier dependency findings are resolved in the manifest, native reflected filter adapter and development prerequisite contract. Native discovery now only scans dedicated depot recipe identity. Own slot-filter configuration and generic transfer receipt bridge remain. ExternalFilter is a saved compatibility marker only: no external configuration discovery, no restoration of unowned filters, no new job eligibility, active jobs must validate endpoints before resuming; explicit safe remove/reconfigure required. Historical Lua/migration scripts may still mention old class paths for external tooling, not native dependencies. Static regression checks scan native Source/Config and verify standalone manifest/prerequisites. Local unity fixes, selector, fluid gates, depot authoring/cooking and actual standalone build/game remain unresolved.

## Follow-up fluid building source pass

The old generated root and solid-only gating findings are superseded in source by native root, three recipe/descriptor authoring pairs, native resource-form compatibility checks, and medium-aware telemetry. Existing selector gap, local unity compilation fixes, editor authoring/reload/cooking, real fluid enforcement and runtime acceptance remain open. Do not claim generated buildings exist until editor receipts and actual cooked entries are inspected.

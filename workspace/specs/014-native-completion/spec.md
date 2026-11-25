# FILN native completion

Status: in progress; not a release. Source requirement: user request dated 2026-11-24.
This feature supersedes the Lua feature pointer for native development only. Preserve the Lua experiment.

## Required behavior

R01: Build against CSS 5.6.1-87, StarterProject 0170e5feece4bebcb4baa2ec4806587300160285,
SML 3.12.1, game CL520140. Execute compiler/SDK probes, UHT, native build, cook and Alpakit.
R02: Keep vanilla solid/fluid station actors; separate saved FILN configuration, one explicit
resource, native conveyor/pipe handling. Change selection only when empty, undocked and unreserved.
R03: Preserve original LOAD/UNLOAD switch and native inventory/fuel UI. Read one explicitly selected requester resource from the working FILTERS workflow; no extra Apply/Register action or custom FILN menu. Preserve ordinary LOAD supply discovery from Lua. A live integration with the installed programmable station is an interim option; a standalone native selector remains a release gate.
R04: One authoritative solid/fluid dispatcher. Compatible resource/medium/vehicle, exact native
vehicle slot capacities, empty requesters, full fresh pickups, partial retained-cargo priority.
Reject unmanaged, player-driven, autopilot-off, active-job or reserved vehicles.
R05: Directed compatible native road distances; nearest eligible provider to requester first,
cheapest empty vehicle trip second. Cached bounded lazy work, topology invalidation, no stock
invalidation, actual route/autopilot readback. No geographical-distance substitution.
R06: Atomic truck/station reservations, native job progression and measured transfers.
R07: Dedicated constructible depot, automatic vehicle enrollment via the original player-selected depot-only route and AP opt-in, exclusive persistent home ownership, automatic parking/refueling/recovery. If the selected depot already has an owner, automatically drive the new vehicle to a free reachable compatible depot; never move the owner, never share a depot, and never override AP-off or player driving. If none exists, leave the vehicle and its route unchanged and report the blocker in telemetry. Native actual fuel only.
R08: Case-insensitive [S] restrictions for both media. No fresh warehouse/warehouse journey;
no unknown-origin retained cargo assigned to a warehouse requester.
R09: Legacy-save migration is external to the released native mod (user scope correction
2026-11-24). Codex/Lua and offline tools may prepare copies independently. No legacy
recipe/class recognition, journal or manifest import, or save-specific behavior in native C++.
Use the same public native station/depot/vehicle enrollment interfaces for every world.
R10: Separate disable, prepare-uninstall, and independently verified load without FILN.
Preserve vanilla buildings, cargo/fuel, routes and all physical connections.
R11: Existing web dashboard for read-only statistics, jobs, depots, routing and diagnostics, with native telemetry and no Lua runtime dependency. No in-game management/administrative panel, label rewriting, vehicle registration or dispatcher/recovery buttons. Diagnostic console and uninstall preparation may remain internal.
R12: Save/reload, server restart, actor destruction, path edits, assignments, retained/partial
cargo, fuel shortages and duplicate prevention. Pure tests never establish runtime behavior.
R13: Verified Win64 Alpakit artifact, metadata, native PE, cooked required assets, SMM layout,
no Lua/UE4SS/DataForge/ST-input dependency or development files. No installation/publication.

## Decisions and conflicts

- Latest user requirements override old configurable thresholds/hysteresis and legacy LOAD/UNLOAD
UI wording in CODEX_TASK. Empty requester and full fresh vehicle load remain mandatory.
- PR #1 ranked total round-trip distance. Authoritative CODEX_TASK rule 6 explicitly ranks
provider→requester first; retain that rule and use depot travel only secondarily. Regression
cases now cover this conflict. Across requesters, stable requester-ID ordering is used by the
portable selection function; a scheduler fairness policy remains to be implemented.
- Canonical source is workspace/mods/FicsItLogisticsNetwork, not ignored StarterProject copies.
- The separate rest-stop recipe/building is mandatory, following the Lua building workflow. Existing registered vanilla home fallback is not an acceptable substitute or completion claim.
- Slot-filter restrictions use public native inventory methods. Their effect on conveyor,
docking and fluid transfers must be established in isolated runtime tests before acceptance.

## Acceptance

Every R requirement requires its traceability row and relevant executable tests to pass.
An absent working FILTERS selector, dedicated depot, automatic relocation, native dashboard telemetry, scheduler, package or independent mod-disabled
load is a failed acceptance criterion, not a skipped success. See tasks.md and validation.md.
All game acceptance uses isolated copies and requires authorization before affecting any
running session. No graphical editor is needed for C++, UHT or command-line packaging.

The external legacy migration workflow is not a native release criterion. It remains unverified; scope exclusion is not a passing migration test.

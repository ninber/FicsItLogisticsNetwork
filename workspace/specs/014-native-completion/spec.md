# FILN native completion

Status: in progress; not a release. Source requirement: user request dated 2026-11-24.
This feature supersedes the Lua feature pointer for native development only. Preserve the Lua experiment.

## Required behavior

R01: Build against CSS 5.6.1-87, StarterProject 0170e5feece4bebcb4baa2ec4806587300160285,
SML 3.12.1, game CL520140. Execute compiler/SDK probes, UHT, native build, cook and Alpakit.
R02: Keep vanilla solid/fluid station actors; separate saved FILN configuration, one explicit
resource, native conveyor/pipe handling. Change selection only when empty, undocked and unreserved.
R03: Native in-game selector and PROVIDER/REQUESTER switches preserving vanilla styling;
ordinary stations retain vanilla UI. Native provider means LOAD; requester means UNLOAD.
R04: One authoritative solid/fluid dispatcher. Compatible resource/medium/vehicle, exact native
vehicle slot capacities, empty requesters, full fresh pickups, partial retained-cargo priority.
Reject unmanaged, player-driven, autopilot-off, active-job or reserved vehicles.
R05: Directed compatible native road distances; nearest eligible provider to requester first,
cheapest empty vehicle trip second. Cached bounded lazy work, topology invalidation, no stock
invalidation, actual route/autopilot readback. No geographical-distance substitution.
R06: Atomic truck/station reservations, native job progression and measured transfers.
R07: Explicit enrollment, home ownership, return/park/hold, native actual fuel only, rest-stop
fuel connections and interrupted-delivery recovery for compatible solid/fluid trucks.
R08: Case-insensitive [S] restrictions for both media. No fresh warehouse/warehouse journey;
no unknown-origin retained cargo assigned to a warehouse requester.
R09: Versioned, repeatable, recoverable legacy migration. Discover and preserve stations,
filters/names/modes, cargo/fuel, routes/node GUIDs, connections, depots and registrations.
Validate before replacement; report unsupported objects. Never migrate an active save.
R10: Separate disable, prepare-uninstall, and independently verified load without FILN.
Preserve vanilla buildings, cargo/fuel, routes and all physical connections.
R11: Native management UI for registry, resources, vehicles, depots, jobs, reservations,
failures, paths/fuel, migration, uninstall and enable/disable. Validated server mutations.
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
- Prefer vanilla station extensions over custom station actors. Existing native home fallback
is a vanilla LOAD station, not an implemented rest-stop building.
- Slot-filter restrictions use public native inventory methods. Their effect on conveyor,
docking and fluid transfers must be established in isolated runtime tests before acceptance.

## Acceptance

Every R requirement requires its traceability row and relevant executable tests to pass.
An absent migration, management UI, automatic scheduler, package or independent mod-disabled
load is a failed acceptance criterion, not a skipped success. See tasks.md and validation.md.
All game acceptance uses isolated copies and requires authorization before affecting any
running session. No graphical editor is needed for C++, UHT or command-line packaging.

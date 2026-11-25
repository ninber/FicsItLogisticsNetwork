# FILN workspace guidance

## Current authoritative scope (2026-11-25, overrides older guidance below)

Use specs/014-native-completion/spec.md and standalone-station-implementation.md. Standalone native SML-only solid AND fluid logistics are required. Remove Smarter Truck Stations entirely, including optional compatibility; its old UI and Lua experiment are historical references only. Native LOAD/UNLOAD labels stay unchanged. One explicit compatible resource selected through FILN's own native-looking selector; no manual player registration, extra Apply/Register, management menus or browser write actions. Dedicated depot construction, automatic AP-on home adoption and relocation of only the new vehicle to a compatible free depot are required. Browser statistics are read-only. Never infer requester identity from stock. Preserve existing Lua test evidence without interpreting it as native acceptance. Reconcile local Codex unity-build fixes and root-module failure before changing native code. Do not touch the original City save, running game, or automate UI. All unresolved requirements remain failing release gates; passing portable tests or header checks are not native gameplay proof.

Remaining notes are historical or apply to the separate Lua experiment; contradictory older enrollment, Monitor-write, UI or dependency instructions do not apply to native release.

Use CODEX_TASK_FicsIt_Logistics_Network.md as authoritative specification.
Native completion: specs/014-native-completion (current feature.json). Lua reference current feature: specs/013-depot-registration (feature.json); storage: specs/012-storage-stations; reachability: specs/011-reachability-audit; blueprint upgrade: specs/010-mk4-blueprints; dispatcher: specs/009-dispatch-throughput; cargo/item board: specs/008-cargo-priority-item-board; orphan recovery: specs/007-orphan-fleet-recovery; dashboard evidence: specs/006-bottleneck-dashboard; blueprint evidence: specs/005-filn-blueprint-library; powered fleet/recovery evidence: specs/004-powered-fleet-dispatch; station upgrade evidence: specs/003-filn-station-upgrade; route/delivery evidence remains in specs/002-lua-route-control and native M0/M1 in specs/001-filn-parking-routing.
Canonical mod source: mods/FicsItLogisticsNetwork. StarterProject is the upstream development
project; install canonical changes using ../scripts/install-filn.sh. Do not treat copied source
inside the ignored checkout as the source of truth.
Inspect version-matched game/SML declarations before API changes. Do not use legacy vehicle
waypoint lists or depend on FIN in core. Do not claim UHT/UBT or game tests passed without
running them. Host tests: bash ../scripts/test-filn.sh. Record blocked runtime acceptance in
mods/FicsItLogisticsNetwork/docs/VALIDATION.md and keep corresponding Spec Kit tasks open.

The user later authorized a Lua/UE4SS experiment to avoid blocked engine access. Its canonical
source and runtime limitations are in experiments/FILNLua/README.md. This exception does not
mark native M1 complete. Host checks: bash ../scripts/test-filn-lua.sh. Keep route writes
disabled by default and do not imply mocked UE4SS tests prove in-game behavior.

For continued Lua development use Scripts/Bridge.lua for UE4SS array/parameter conversions and strict home audits. Actual discovery/inspection is proven; new route writes must be tested only with --route-bridge or the pinned-fixture --depot-journey mode on an autosave-disabled disposable snapshot, and readback/restoration evidence must be recorded.

Role is always native LOAD/UNLOAD. Requested item comes from the in-game programmable requester filter. The user authorizes ordinary LOAD providers when their cargo contains only that requested item; never learn request identity from inventory. Stock source is station inventory. Requesters dispatch only when empty; providers must hold the assigned vehicle's full item-specific truckload. No configurable stock thresholds or hysteresis. Never revive inventory-based item learning or implicit demand thresholds. Genuine Build Gun upgrades need native hologram/state migration proof; descriptor reuse is not upgrade support.

Offline save migration exception explicitly authorized by the user: infer a fixed item from a clear station name, or from exactly one cargo type in a default-named station inventory. Preserve existing programmable filters; skip ambiguous, mixed or conflicting stations. This does not permit runtime requester item learning.

Current fuel requirement: rest stops support native refueling from player-supplied fuel inputs. Fuel-port identity comes from native mFuelConnections, not connector-name guesses. Cargo belts remain forbidden. User chose manual fueling and a new save; do not seed synthetic starter fuel in subsequent tests. Native home→provider→requester→home and four-vehicle delivery/refueling/return/hold passed in feature 004. Stationary refueling also passed with cargo dispatch paused. Preserve the old source; the corrected checkpoint and native receipts are in specs/004-powered-fleet-dispatch/validation.md.

Blueprint library safety: originals must remain byte-identical; create FILN-prefixed copies only.
Live blueprint category mutation crashed during feature 005. BlueprintLibrary.organize is deliberately
blocked. Use verified offline category-only autosave migration, backups and full-body roundtrip checks;
let native loading rebuild categories. Never call the previous live refresh/struct-array write path.
Native category FString returns need Bridge.text. Generic blueprint station filters remain blank unless
an existing explicit filter was present; do not infer requested items from a generic factory template.

User UI-testing preference: do not automate build-gun previews, keyboard/mouse input, menu navigation or player UI changes. Ask the user to inspect visual/placement behavior manually. Read-only logs/state and host tests remain authorized. Automated previews visibly disrupted the interface in feature 005 despite passing native hologram checks.

Dashboard safety: manual Steam overlay navigation to http://filn.localhost:8765/ works.
FGNetworkLibrary.OpenWebURL calls returned false and the third repeated manual shortcut request
was followed by a freeze/crash with a UE4SS access-violation stack. Causality is not yet isolated.
Dashboard.lua deliberately makes no native engine calls. Do not re-enable that browser-opening
path or automatically test UI. The dashboard remains usable via manual overlay navigation.

Dashboard lifecycle preference: the user explicitly rejected login startup of the HTTP server.
Use mod heartbeat activation (dashboard-mod.txt via the systemd path unit); the service itself
has no Install section and must not be enabled at login. The server stops when mod heartbeats
expire. Only systemd's file watch is registered at login. Do not replace this with an always-on
server or silently edit Steam launch options. Current old-module sessions can use a captured
process-lifetime fallback only while no mod heartbeat file has ever been created.

User testing preference after feature 007 interruption: do not shut down or restart an active
player session to run disposable tests without asking first. Prior broad automation permission
does not replace this check. Before any authorized shutdown, issue native autosave, require PASS,
verify the resulting real autosave/hash and make the saved progress explicit to the user.
The feature 007 native test was cancelled at the user's objection; do not resume it automatically.
Previous installed Lua files were restored; feature 007 changes remain local and unverified natively.

Player/session restoration requirement: the user's actual player is DefaultImaginati. Direct
GameDefaultMap/LocalMapOptions (?listen?loadgame=...) launch loaded the correct city but created
an active Player identity rather than the user's character. Do not use that shortcut to restore
production gameplay or claim restoration based only on LoadMap/save filename. Use the normal
menu/session load path with manual UI control by the user, and verify DefaultImaginati is active.
FGLocalPlayer.h explicitly says saves should load through AFGAdminInterface::LoadGame. Do not
force possession, rename Player, or rewrite account/player records to disguise the mismatch.

Latest active scope: continue FILN implementation and host tests; leave the game/player session
alone. Do not act on the earlier pending restart question: the user cancelled that direction.
Native acceptance/deployment of feature 007 remains pending, not an excuse to restart gameplay.

Cargo-priority exception explicitly requested by the user: eligible idle FILN trucks already
carrying the matching item take priority over fresh provider pickups. These direct deliveries
may be partial truckloads; fresh pickups still require a full vehicle load at a single provider.
Requesters still require an explicit item filter and empty station inventory. Do not commandeer
active jobs, ordinary trucks, player-driven or autopilot-off vehicles.

Progress-loss prevention: numbered autosaves rotate and are not a durable checkpoint. Before
any explicitly approved game interruption, verify native save PASS, then copy the resulting save
outside the rotating SaveGames set into a timestamped workspace/test-results backup and verify
its hash. Make the durable backup path explicit before shutdown. Never treat the existence of
an autosave alone as preserved progress. Leave player identity issues aside unless requested.

Storage rule: user marks warehouse stations with [S] in names. Never create a new pickup
pair when both provider and requester are tagged (case-insensitive). Warehouse/factory pairs
remain eligible. Finish in-flight recovery safely; unknown-origin retained cargo cannot be newly
assigned to tagged storage requesters. No inventory thresholds are introduced.

Explicit Monitor-control exception: user requested registering new trucks parked at unclaimed
FILN depots. Monitor may queue only that fixed action via authenticated same-origin POST. Native
handler must revalidate unique nearby rest-stop/empty stopped truck and read back home-only/APon.
Do not commandeer scheduled or driven vehicles. Queued requests are not native success. Existing
game-interruption, UI and possession restrictions remain.

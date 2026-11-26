# FILN Lua / UE4SS experiment

For automatic update → Steam launch → latest-save load → ping/scan/inspect → shutdown with
machine-readable results, see [AUTOMATION.md](AUTOMATION.md) and
`python3 scripts/auto-test-filn-lua.py` from the outer project. Host execution via approved escalation is now verified; the latest unattended runtime cycle passed.

Status: user installed UE4SS/FILNLua; loader startup, terminal inbox receipt and game-thread
scan execution confirmed from the actual UE4SS.log on 2026-10-24. The first scan exposed
an array adapter bug (`ForEach` missing); source now handles copied Lua arrays as well as
remote TArray objects. Actual unattended enumeration of **19 vehicles and 52 stations**, plus all 19 vehicle route inspections, now passes. Reversible route mutation/readback/restoration now passes in game; actual driving and parking remain **unverified**.
The user authorized this experiment after the native CSS-engine setup was blocked. The native
SML plugin is preserved. This experiment is not a completed M1 or a production fleet scheduler.

## First runtime test

Close Satisfactory. From the outer SatisfactoryModding folder, run in your normal terminal:

```bash
python3 scripts/setup-filn-lua.py --game-dir "$HOME/.local/share/Steam/steamapps/common/Satisfactory"
```

No Epic/linker account is required for these public binary downloads. The script fetches
UE4SS `UE4SS_v3.0.1-1181-g83330635.zip` and its release's custom configurations, verifies
pinned SHA-256 values and sizes, stages them in `toolchains/FILNLua-UE4SS`, overlays the
Satisfactory configuration, and copies the loader beside the actual shipping executable.
The pinned metadata is in `ue4ss-assets.json`. `experimental-latest` is mutable: if a pinned
asset disappears, stop and re-audit the new binary/config pair rather than substituting stable
3.0.1 (the experimental branch provides UE5.6 support). The binary snapshot may still have bugs.

Detected game: Steam app 526870, build ID 25782416. Actual executable:
`Satisfactory/Engine/Binaries/Win64/FactoryGameSteam-Win64-Shipping.exe`.
The old `FactoryGame/Binaries/Win64` path contains game DLLs, not this install's executable.
Existing SML, BetterJunctions, TruckPassThrough and LiftPassthroughFix plugins were detected;
coexistence with UE4SS and these mods has not been verified.

Add this Steam launch option (retain any existing options appropriately):

```text
WINEDLLOVERRIDES="dwmapi=n,b" %command%
```

Launch the game. Check `Engine/Binaries/Win64/ue4ss/UE4SS.log` for UE4SS initialization and
`[FILN-Lua] loaded`. Failure to load the loader is a compatibility issue, not a Lua dispatcher
failure. If the game crashes before the mod-load line, disable the proxy and inspect that log
before trying any vehicle commands. A missing signature, incorrect modular DLL match or
configuration offset may require a new UE4SS build/config audit.

Load a disposable single-player/host save, then use the Unreal game console:

```text
filn_lua.scan
filn_lua.inspect V1
```

The installer enables the bundled ConsoleEnablerMod if present. A UE4SS Lua console is not
the same as the game's Unreal console; these registered commands require the Unreal console.
The optional GUI is enabled with EngineTick rendering; Proton GUI behavior is unverified.

Expected scan output: V1/V2/... vehicle IDs and S1/S2/... station IDs with full object paths
and copied GUIDs. Inspect prints the route, target index, autopilot flag and error value.
These operations do not write vehicle routes. IDs are local to this scan/session, not durable
identities. No scanning happens automatically on startup or per frame.

### Command not recognized despite successful mod startup

The initial viewport-only console handler did not receive this game's console commands.
The updated script uses RegisterConsoleCommandGlobalHandler and logs each received command.
Close Satisfactory, then update the installed script from the project folder:

```bash
bash scripts/update-filn-lua.sh
```

This preserves installed Config.lua and backs up previous scripts. Restart the game and enter
`filn_lua.ping` in the Unreal console, then `filn_lua.scan`. A received message appears in the
console; detailed results appear in UE4SS.log. These are not SML chat commands.
Global registration and ping receipt passed mocked-host regression checks; the corrected
handler still requires a real in-game retest.

Actual retest on 2026-10-24: global handlers loaded, but neither console ping reached Lua.
Unreal console interception is therefore still nonfunctional on this game/loader combination.
After updating and restarting again, use either independent entry point:

- With the loaded save focused, **Ctrl+Shift+J** posts requests to native game chat; open chat with **T** to reread. FILN does not bind F keys. Ping/scan remain available through console or terminal commands.
  Results appear in UE4SS.log, not the Unreal console.
- From a normal terminal in this project folder:

  ```bash
  bash scripts/filn-lua-command.sh ping
  bash scripts/filn-lua-command.sh scan
  bash scripts/filn-lua-command.sh inspect V1
  ```

The terminal helper appends validated commands to the installed Scripts/commands.txt.
The mod reads at most 4 KiB/16 lines each second, queues game work on the game thread,
and skips historical contents on startup to prevent replay. Send commands after the game
and script have loaded. It never evaluates file contents as Lua. Both transports share the
same route-write gate and validation. Terminal inbox commands are now verified in the actual game; the direct keypress entry point remains unverified.

## Opt-in route-control test

First establish that UE4SS loads and all read calls succeed. Close the game, set
`AllowRouteWrites = true` in the installed `ue4ss/Mods/FILNLua/Scripts/Config.lua`, and restart.
The canonical default remains false. Do not rerun the full installer over an existing loader;
it refuses to overwrite it. To update the experiment source, copy this `Scripts` directory
to the installed mod while the game is closed.

Prepare three distinct powered, path-connected stations: dedicated empty LOAD home (H),
stocked LOAD provider (P), empty UNLOAD requester (Q). Home must have no cargo conveyors or
pipes; optional fuel is allowed. Empty the truck cargo, physically dock it at H, and disable
autopilot. Ensure the native game can drive the directed H→P→Q→H road loop for this vehicle
preset. Never create a single-station route.

Using IDs from the new scan (substitute actual IDs):

```text
filn_lua.scan
filn_lua.pair V1 S1
filn_lua.plan V1 S2 S3
filn_lua.apply V1
filn_lua.inspect V1
filn_lua.start V1
filn_lua.status
```

Plan checks all three directed legs using the game's preset-compatible pathfinder, without
writing a route. Apply atomically reserves the truck/endpoints, saves a session-only original
route, writes provider→requester→home, and compares readback. It does not enable autopilot.
Start is explicit and revalidates the legs. FGuid/TArray input and out-parameter marshaling
are supported in the general UE4SS bridge but **not verified for these Satisfactory functions**.
An exception retains reservations and requests stop; a native loader crash cannot be caught
by Lua pcall. No internal target-index setter or legacy waypoint code is used.

The bounded watchdog observes docking, transitions through loading/unloading/returning, then
requests autopilot off at home. Idle requires stopped home docking and empty vehicle cargo.
Real transfer success is not inferred from undocking. Verify actual inventories yourself.
Test a second trip only after the first returns and remains parked for several game ticks.

```text
filn_lua.stop V1
filn_lua.reset V1
filn_lua.restore V1
```

Stop requests autopilot off and retains reservations in Recovery. Reset requires confirmed
physical home parking, stopped autopilot and empty cargo. Restore requires home parking and
restores the original route from this session, verifies readback, and leaves autopilot off.
These commands do not magically move the truck back home. A failed bridge disables further
apply/start until a full game restart and manual inspection.

## Persistence and performance limits

**Do not save/reload or hot-reload during an experimental journey.** Game routes/autopilot
can persist, but this experiment's ownership, original routes and reservations do not. Stop
and restore managed vehicles before saving or exiting. If interrupted, stop/recover the truck
manually using native game controls; the Lua session cannot promise safe saved-job recovery.
Durable IDs/save integration are unfinished. Production M1 save/load acceptance remains blocked.

Only an explicit scan uses FindAllOf, for one authoritative vehicle subsystem. Dispatch
queries three path legs; no inventory callback runs pathfinding. The active-job list is
bounded to 16 observations per timer pass, with one pending game-thread callback. No per-frame
world scans or position polling. The async timer itself only schedules game-thread work.
Missed brief docking causes recovery or timeout, not assumed completion. At fleet scale,
observation latency requires a reliable dock-event bridge; this polling proof is not that bridge.
Timeout is conservative wall-clock time, including paused time.

There is no nearest-provider cache, automatic stock dispatcher, transfer telemetry, custom
parking asset or FIN dependency. The future lazy cache architecture remains in the native
Spec Kit plan. No claim about Lua meeting or failing the fleet performance budget is made.

## Validation

```bash
bash scripts/test-filn-lua.sh
```

Host LuaJIT (Lua 5.1-compatible syntax): **1,694 checks passed**. Shared pure code covers GUID
copy/validation, exclusive ownership, atomic endpoint reservation, recovery, release and
journey transitions. A mocked UE4SS host runs the actual main.lua command flow through
scan, read-only rejection, pairing, disconnected-leg rejection, route apply/readback,
start, complete observed journey, restore, second assignment, stop/recovery and home reset.
Mocks cannot establish UE4SS marshaling, function availability or real game behavior.
Regression checks also cover direct key dispatch, terminal inbox consumption, historical
command skipping, no replay on repeated polls, truncation handling and unknown-command rejection.
The main.lua integration-shaped test exercises copied Lua arrays containing parameter wrappers
on the first journey, remote TArray-style wrappers on the second, and direct Lua values during
inspection. Actual scans exposed both missing ForEach and then missing GetFullName: copied
array elements must also be unwrapped with get(). That conversion is now implemented and
regression-tested; runtime enumeration now passes (see AUTOMATION.md).

Runtime follow-up: the scan listed 19 actual vehicle identifiers, then rejected a station GUID
word. GUID conversion now accepts both signed-int32 and unsigned-int32 representations of
the same 32-bit words, with explicit malformed-value diagnostics. Scanning lists stations
with unavailable/zero GUIDs individually rather than aborting; enrollment still rejects them.
Station enumeration now passes with zero unavailable GUIDs; route control still requires runtime testing. The terminal helper
now waits up to six seconds and prints new FILN log replies directly.
UE4SS uses Lua 5.4; its actual interpreter was not run here.

Single host samples for pairing/reserving 100/500/1,000 vehicles with 300 endpoint slots:
0.274 / 1.566 / 2.694 ms. These are bookkeeping exercises, not per-dispatch or game-thread
measurements; no cache work, memory estimate or native autopilot cost is included.
Python compilation, valid ZIP extraction and three unsafe ZIP-path rejections passed.
A synthetic offline installer test also passed: correct shipping-executable directory,
configuration/mod staging, console-helper enablement and refusal to replace an existing stage.
That fixture does not validate the real downloaded ZIP layout.
The real download attempt failed: `curl: (6) Could not resolve host: github.com`.
The sandbox also cannot write the installed Steam game directory. Initial loader installation
was performed by the user. Its actual log now confirms UE4SS and FILNLua startup; no command
receipt appeared for the original handler. Console interception still does not work, but the inbox transport now passes in-game. Discovery and route inspection passed automatically; driving and job save/recovery remain NOT RUN.

## Remove the experiment

Close the game. Remove/rename the installed `dwmapi.dll` proxy and remove this launch override
while retaining unrelated overrides. This disables UE4SS; preserve its folder/logs for diagnosis.
Removing the loader does not restore routes already changed in the game: restore/stop first.

## Sources

- Official prebuilt releases: https://github.com/UE4SS-RE/RE-UE4SS/releases/tag/experimental-latest
- Satisfactory configuration: https://github.com/UE4SS-RE/RE-UE4SS/tree/main/assets/CustomGameConfigs/Satisfactory
- Lua mod layout: https://docs.ue4ss.com/guides/creating-a-lua-mod.html
- Console bridge: https://docs.ue4ss.com/dev/lua-api/global-functions/registerconsolecommandhandler.html
- Game-thread execution: https://docs.ue4ss.com/dev/lua-api/global-functions/executeingamethread.html
- UFunction/out-parameter implementation inspected: https://github.com/UE4SS-RE/RE-UE4SS/blob/main/UE4SS/src/LuaType/LuaUObject.cpp

No new general-purpose license is granted for FILN-owned code in this revision. Earlier grants remain effective. UE4SS binaries/configuration retain their own upstream license, included with the downloads. See [publication compliance](../../PUBLICATION.md).

## Next fixture: parking bays

The agreed target is a repeatable foundation parking pad with a rest-stop depot, one truck
per bay, with native refueling from user-supplied fuel inputs. This buildable is not available yet. Road-node identity is exposed,
but a working stop/hold contract and placeable composition still need runtime verification.
See [parking design](../../specs/002-lua-route-control/parking-bay.md) and
[validation](../../specs/002-lua-route-control/validation.md).

Script-generated prototype: [DataForge pack](DataForge/README.md). A disposable in-game test
proved a separate rest-stop class/recipe/unlock and building availability. The dependency and
pack were removed after testing. Station visuals are temporary; placement, actual holding,
arrival hooks and release into a changed dispatcher route remain unverified. The isolated
RestStop.lua controller has 26 host checks; it is not yet connected to main.lua gameplay.

## Persistent rest-stop installation — 2026-10-24

The dependency, pack and normal-session unlock now remain installed in the game folder.
`python3 scripts/install-filn-rest-stop.py` installs/updates them with archive hash checks and
pack/script backups. The user confirmed the FILN station is visible in the menu. Placement
and road attachment must be established before dispatcher/journey tests. The hold controller
is still isolated; this prototype is not ready for automatic vehicle dispatch.

Selection-crash correction: the custom descriptor/recipe now use the original cooked station
actor to avoid broken generated actor component templates. Native hologram creation passed
on a disposable save (20261024T115112Z-a5b0a49f). Normal game was reopened and left running
for placement/road tests. See DataForge/README.md for current limitations; vanilla cargo/fuel
behavior is retained until per-instance rest-stop integration is implemented.

## Explicit station configuration and upgrade work
The observed-item/25% heuristic and subsequent browser/threshold experiment are
retired. Configuration comes from programmable stations' in-game filters; role
comes from LOAD/UNLOAD. See the truckload policy below and
../../specs/003-filn-station-upgrade for runtime gates.

Lua retrofit alternative: Scripts/StationRetrofit.lua keeps the original actor and road GUID,
sets native cargo-slot filters, refuses occupied/conflicting stations and supports rollback.
This is a backend, not a Build Gun upgrade or finished settings UI. Automated reversible
native checks are available with --station-retrofit; docking and reload need separate proof.

### In-game station filters and truckload dispatch

The browser settings experiment has been removed. Use Smarter Truck Stations'
Programmable Truck Station and its in-game filter UI. FILN reads `FilterItem`;
select exactly one requested item at each requester. LOAD remains provider, UNLOAD requester.
Ordinary LOAD providers are eligible when their cargo contains only that item.
Requesters with empty/multiple distinct filters are not eligible. Existing TSV/Lua
settings from the rejected browser prototype are ignored.

Demand means the requester station inventory is empty. Provider stock must fill
the assigned vehicle's measured item-specific cargo capacity, and the requester
must have room for that full load. No activation/stop settings or hysteresis.
A delivery must observe a full truckload and return empty to its home bay.

Native filtered cargo delivery and depot return have passed on a single-station conversion.
Bulk offline conversion subsequently passed all 38 native station identity/filter/role/road
checks. This is save-copy migration; in-game Build Gun replacement remains unavailable.
See specs/003-filn-station-upgrade/validation.md for individual acceptance limits.

### Programmable building missing from the menu

The installed mod defines a separate HUB milestone. FILN now loads the existing
Smarter Truck Stations assets and grants that specific schematic on the host,
then checks recipe/building availability, Build Gun listing and descriptor
category. The verified category is Vehicles. This does not replace any station.
Native recipe availability was proven on a disposable save; actual menu rendering
and the filter widget remain separate runtime checks.

### Current automatic prototype

Installed normal-session configuration enables AllowAutoDispatch. Canonical
default stays off for new installs. Requester: programmable in-game single-item
filter plus empty station inventory. Provider: ordinary LOAD station holding only
that item and at least the assigned truck's cargo capacity. No stock thresholds.
Search samples one station per 250ms pass, skips busy/unassigned homes, and caches
road distances by native network/revision. This avoids the old periodic full sweep.
A real 4800-ingot delivery/return/12-second home hold passed. Large-fleet frame
times remain unmeasured. User confirmed smooth play after the search rewrite.

Autopilot stays enabled on the parked home-only route (TooFewStations is expected
there). Switching it off detaches the truck and releases FILN ownership without
rewriting its route. Entering a truck is not an error; parking changes are deferred
while occupied. FILN and its tests never force player entry/exit. Re-enroll by
returning an empty truck to its home, selecting only that bay, and enabling autopilot.
A write-ahead journal prevents an unattended repeated trip after a mid-trip reload;
full automatic in-flight recovery remains unfinished.

FILN Logistics Station content reuses the existing cooked programmable actor and
widget; the local source fork is in workspace/forks/SM_TruckStationSettings and
has not been independently rebuilt/cooked. Install the extension while closed
with `python3 scripts/install-filn-stations.py`. Existing-city conversion uses
[scripts/save-tools](../../../scripts/save-tools/README.md) and writes a new save,
preserving station/node/component references. It is not an in-game Build Gun upgrade.

### Clean automatic-delivery baseline
Run `python3 scripts/auto-test-filn-lua.py --automatic-delivery --isolate-traffic --cargo-audit --journey-timeout 600 --start-timeout 900` against the pinned fixture. The runner backs up the source, loads a disposable copy with autosaves disabled, and pauses ordinary vehicle autopilots while leaving the depot truck active. It verifies every route remains unchanged. The source city and its normal traffic are preserved. This isolates one delivery; it does not validate scheduling alongside ordinary trucks or a multi-vehicle FILN fleet. Multiple new parking bays are not required for the single-truck baseline.

### Home fuel service (feature 004)
The planned native route is home → provider → requester → home. Logical provider/requester reservations are unchanged. Fuel input belts are allowed at the rest stop; cargo connections are rejected. The dispatcher waits for the native return docking cycle before restoring the home-only idle route. An empty truck is eligible only if its powered home contains fuel; startup waits have a bounded timeout. Native proof passed for four vehicles on the manually fueled source: full deliveries, native home refueling, return and hold; stationary refueling with cargo dispatch paused passed independently. No artificial starter fuel is used for that acceptance.

### Verified four-truck checkpoint
`City_FILN_Stations_20261025_145945` is the new fuel-corrected copy. The first fleet passed four complete 4800-item deliveries, native refueling, empty home returns and holds. The four fuel-branch splitter outputs were None; they now permit fuel flow while bypass rules stay unchanged. Fuel goes into dedicated refueling inventories, with rest-stop cargo empty. `bash scripts/filn-lua-command.sh depot_fuel` reads the actual fuel/power state. The first fleet dispatcher is enabled in the reopened corrected city; stationary refueling also continues when new cargo dispatch is paused. Details and source preservation: specs/004-powered-fleet-dispatch/validation.md.

Recovery FSM, diagnostic fields, fuel gate and request view: [RECOVERY.md](docs/RECOVERY.md).

### Bottleneck dashboard (feature 006)

For startup with the game mod, run `bash scripts/install-filn-dashboard-service.sh` once.
This is installed on the current machine. The mod writes `dashboard-mod.txt` every five seconds;
a systemd file watch activates the loopback server. The server exits when that heartbeat stops
for 30 seconds. The HTTP server is not enabled at login; only the file watch is registered there.
The current pre-update game session uses a process-lifetime fallback until its next normal restart.
No Steam launch options are changed.

Run `bash scripts/start-filn-dashboard.sh` from the repository after installing the updated Lua mod
and restarting the game. Open Steam's overlay browser manually and enter
**http://filn.localhost:8765/**. The user confirmed this address loads; the numeric loopback URL
was refused by the overlay browser.

**Automatic opening is disabled.** Repeated Ctrl+Shift+J tests through the game's
`FGNetworkLibrary.OpenWebURL` helper returned false, then a subsequent request was followed by a
freeze/crash. The guard now makes no native browser calls; pressing the shortcut only requests
telemetry and logs the manual URL. The crash root cause is not conclusively isolated. No external
host, account or configuration website is involved.


The default Bottlenecks view shows empty programmable requesters, single-provider stock versus a
full vehicle load, deficits, combined stock, assignments and recent dispatcher route evidence.
Fleet shows cargo, dedicated fuel, job stages, home, recovery and scheduler reasons. Inventories
includes nonempty stations and unconfigured stations, with separate cargo/fuel inventories.
Search and table headers filter/sort the results. Uncheck “Empty requesters only” to see configured
requesters that still have stock.

The dashboard cannot change stations, inventory or dispatch settings. Providers still need one full
truckload at one station; requesters activate only at zero station inventory. Net stock trends do
not measure machine production or external buffers. Batch-fill estimates are not delivery ETAs.
Capacity samples represent active vehicle class/slot combinations, with per-slot customizations a
possible source of differences. Route checks are scoped to the tested home and shown with their age.

Sampling is demand-driven: a local page heartbeat enables one vehicle/station/capacity task per
100 ms, followed by a five-second cooldown. Closing the dashboard stops new scans after the heartbeat
expires (up to 30 seconds; manual open allows up to 60 seconds). Each completed snapshot includes
per-object sample timestamps and errors. Hundreds of objects take longer to collect; the UI marks
snapshots older than 45 seconds stale. It does not hide incomplete samples or fabricate stock.

Service output and PID: `workspace/test-results/dashboard/server.log` and `server.pid`.
On this Bazzite installation it is activated by the running mod; stop activation and serving with
`systemctl --user stop filn-dashboard.path filn-dashboard.service`.
On systems without a user systemd service it uses nohup; verify the recorded PID belongs to
`scripts/filn-dashboard.py` before terminating it. Remove automatic activation with `systemctl --user disable --now filn-dashboard.path` and then
`systemctl --user stop filn-dashboard.service`.
The service binds only `127.0.0.1:8765` and serves a fixed set of read-only endpoints.

Fleet diagnostics distinguish **Blocked: cargo remains** from an available depot truck.
Such a vehicle cannot receive a fresh full-load job until its cargo is empty. An observed old
FILN route may be shown for an opted-out truck, with requester free space and the cargo amount
that cannot unload now; this display does not re-enroll it or change its controls. Autopilot off
remains an explicit player opt-out. Free-space values are current observations, not proof of
station inventory at the original dispatch time.

### Fleet recovery candidate (feature 007)

The current workspace adds bounded reconciliation for exact FILN four-stop routes that have
lost their dispatcher observer. It validates depot identity and native GUIDs/roles, uses the
requester's explicit filter, and checks carried cargo. Ordinary two-stop routes, AP-off trucks,
player-driven trucks, ambiguous home claims and conflicting leases are left unchanged with a
reason recorded. Automatic orphan adoption follows the automatic-dispatch pause gate.

Recovery validates leftover cargo before restoring the home-only route. Missed-stop and partial
unload errors retain ownership and replan remaining cargo delivery instead of restarting pickup.
A loaded truck recovering from a native fuel fault also avoids another provider loop. Failed
internal route writes and failed journal cleanup keep an observer and visible retry state.
Reconciliation preserves its cursor across registry refreshes so a large fleet's last trucks
are not starved. Journal matching uses the shared cached route registry and freshly checks the
single matched vehicle before restoring it.

The dashboard candidate adds **Untracked FILN** counts and per-truck reconciliation reasons,
recovery attempts, retry times and latest retry errors. It distinguishes ordinary routes,
player opt-outs, incomplete FILN schedules and trucks blocked by retained cargo.

These feature 007 changes are **workspace-only** after the cancelled native test. Host tests
pass; native docking/remaining-cargo/return acceptance and deployment are pending. Continue
using the existing installed mod until this candidate has been validated. No automatic game
shutdown/restart or direct-map production restoration is part of this work.

### Cargo-first dispatch and item board (feature 008 candidate)

The workspace candidate prefers an idle, depot-only FILN truck carrying one matching item over
an empty truck collecting a fresh load. Partial retained loads are allowed. The destination must
still be an explicitly filtered, empty UNLOAD station with enough capacity. The truck delivers
directly to that requester and returns home; fresh pickups still require a full truckload at one
provider. Disabled autopilot, a driver, active work, insufficient fuel or unreachable paths prevent
selection. Existing route caches and bounded sampling remain in use.

The default **Items** view pairs green provider and red requester panels for each item, including
station inventories/capacities, assignment IDs, waiting reasons and observed net rates. Disabled,
occupied and stocked requesters remain visible. Unknown filters are kept in an Unconfigured group.
Fleet, Bottlenecks and Inventories remain available. The read-only monitor cannot set filters.
The backend recognizes retained-cargo availability only when telemetry advertises the new runtime
capability; an older installed mod is never assumed to support it.

Extracted artwork is no longer bundled. The historical Lua monitor uses the original neutral placeholder; the native runtime generates its own local cache from the installed base game. No external browser requests or game browser-opening calls are added. Navigate manually to
`http://filn.localhost:8765/` in Steam's overlay browser.

Feature 008 and the preceding feature 007 recovery candidate are installed after explicit
approval, but **not yet verified in the game**. Host tests use mocks and temporary telemetry; actual native delivery, reload behavior
and visual layout need acceptance in a safely saved session. Do not restart an active session
without checking with the user. See `../../specs/008-cargo-priority-item-board/validation.md`.

### Dispatch throughput (feature 009)

Inventory catalogs now refresh in the background after the first complete scan; registry refresh
no longer discards an active ranking. Successful starts allow another ranking immediately.
The scheduler still samples at most one station and compares one truck per pass. A 105-station,
10-truck host simulation fills the fleet within 24.75 seconds after the first start; this is a
mock timing result, not a native fleet-performance measurement.

A fresh pickup reserves its provider until observed loaded departure, allowing the remaining
stock to serve another requester while the first truck delivers. Requesters stay exclusive.
Lease cleanup checks the owner so an older job cannot erase a newer job's provider reservation.
The dashboard displays catalog/ranking progress, and completed pickup legs no longer count as
current provider assignments. Native throughput acceptance remains pending a safe manual reload.

### Reachability diagnostics (feature 011)

A failed delivery may involve the provider leg even when the requester has good roads. New
request evidence names the provider and failed leg. Station item panels include native docking
node road-port counts when available. RoadCache now invalidates sparse projections when the
station registry changes, preserving the graph and lazy cache while allowing new station GUIDs.
Host regressions pass; native acceptance of this candidate remains pending a user-controlled
restart. Investigation evidence is in specs/011-reachability-audit, including an isolated full
provider and two connected requesters; this does not establish a native game bug.

### Storage stations: `[S]` warehouse rule (feature 012)

Put `[S]` anywhere in both warehouse station names (for example `[S] Iron IN` and `[S] Iron OUT`).
FILN forbids new deliveries between two tagged stations, including across different warehouses.
Tags are case-insensitive. Warehouse↔factory deliveries work normally; LOAD/UNLOAD and explicit
item-filter rules still apply. There are no new stock thresholds.

Catalog selection and live pre-dispatch name validation both enforce the rule. Existing delivery/
recovery jobs finish normally. Because retained cargo has no verified pickup origin, new direct
retained-cargo deliveries target ordinary requesters only. The dashboard explains excluded
warehouse supply once the updated runtime is loaded. Native acceptance remains pending.

### Register parked trucks (feature013)

FILN Monitor now has **Register parked trucks**. An explicit click assigns one home stop and
turns autopilot on for empty, stopped, unoccupied trucks with no schedule parked within8m of
an unclaimed FILN rest stop. Ambiguous nearby depots/trucks and existing schedules are skipped.
Runtime rechecks exact identities, recipe, cargo, driver, speed and claims before writes.
Power/fuel and route eligibility still control subsequent dispatch.

The button needs the updated Lua handler loaded after a normal restart. It shows queued/running
status and per-truck results; queueing does not mean success. Current requested12 registrations
are queued for next normal load. No game restart is automated. The local server now has one
explicit registration-control endpoint, protected by same-origin and per-server nonce checks.

# Unattended runtime regression runner

`python3 scripts/auto-test-filn-lua.py` from the outer project starts a complete test cycle;
there are no prompts, menu clicks, manual scans or manual log-reading steps once it starts.

1. Check effective write access and graphical Steam availability before changing/stopping anything.
2. Run host Lua regression checks.
3. Select the newest gameplay save for this Steam owner (or common saves), excluding root
   ServerManager metadata, nested backups and previous FILN automation snapshots.
4. Back up saves, request the running shipping process to terminate with SIGTERM, wait for exit.
   Do not forcibly kill an unresponsive process or update files while it remains running.
5. Run update-filn-lua.sh, verify installed-source hashes, and force read-only experiment config
   temporarily. Preserve the operator config for restoration.
6. Copy the latest save byte-for-byte into a unique common test snapshot; record source SHA-256.
7. Launch Steam app 526870 with the documented GameDefaultMap/LocalMapOptions loadgame override,
   avoiding the main menu. Existing Steam launch options for UE4SS still apply.
8. Require a unique per-run Lua startup token, then ping via a correlated inbox request.
9. Verify the actual game LoadMap log names this snapshot, wait for the authoritative vehicle
   registry, scan and inspect every discovered vehicle. At least one vehicle is expected by
   default, matching the existing fleet; configure --min-vehicles for a different test scene.
10. Stop the test process without requesting a save, restore prior operator config, remove the
    temporary snapshot/token and retain source, backup, command traces and logs in test-results.

The source save is never intentionally overwritten, and its SHA-256 is checked afterward.
The game can still perform normal autosaves during a test; current/common saves are backed up
before launch. The runner does not silently restore changed player saves or kill all of Steam.
Game shutdown does not save unsaved play progress. The user explicitly authorized unattended
restarts/loading of the last persisted save for these test cycles.

## Machine results

Each run writes `workspace/test-results/<UTC timestamp>-<run ID>/report.json` with PASS, FAIL
or BLOCKED, phase, selected save/hash, launch arguments, command outcomes and counts. Queue
receipt alone is never PASS. Unique `AUTOTEST_SESSION` and `AUTOTEST_RESULT` markers prevent
old logs or another command from satisfying the run. Missing loader, timeout, Lua errors,
incorrect/empty fallback world or modified source save fail the run. Existing logs are retained
as *-before.log before starting a fresh log. Scripts, sources and configs are backed up.

Default runtime scope is read-only ping, scan and inspection. It does not automatically pick
arbitrary player stations as home/provider/requester or claim delivery/save-recovery acceptance.
Those require an explicit fixture describing station roles and subsequent route-control work.
The existing Lua save/reload limitation remains; no production journey is started by this runner.

## Validation on 2026-10-24

- 1,694 Lua host checks passed, including per-run tokens and correlated PASS/FAIL responses.
- Ten Python tests passed: full simulated update/launch/load/scan/inspect/shutdown lifecycle,
  Lua-error failure, empty-world failure, preflight before process mutation, save selection,
  launch-argument validation, log rotation/partial-line handling, delayed initial ping, bootstrap detection and Wine cleanup limited to this game/prefix.
- Python syntax compilation passed.
- Initial sandbox runs were blocked, but the updated approval policy allowed host escalation.
- First host attempt auto-loaded the snapshot but timed out waiting for ping while the game
  thread was still loading. Fixed by using the startup budget for the initial callback.
- Relaunch attempts exposed a remaining FactoryGameSteam.exe bootstrap and Wine crash/device
  helpers. Fixed shutdown to include both game executables, then close only wineservers matching
  SteamAppId=526870 AND the selected Proton prefix. Steam itself and other prefixes are untouched.
- A recovered live cycle passed: report `workspace/test-results/20261024T104618Z-eca260ff/report.json`.
- A subsequent **clean, fully unattended end-to-end cycle PASSED** with the updated runner:
  `workspace/test-results/20261024T104959Z-511841cf/report.json`.
  Latest persisted gameplay save selected at that time was `City_autosave_0.sav`; a unique
  byte-for-byte common snapshot was automatically loaded via the map option override.
- Actual results: ping + scan + 19 vehicle inspections = **21 correlated PASS responses**;
  **19 vehicles, 52 stations, zero unavailable station GUIDs**. Native route GUID readback worked.
- Source save SHA-256 unchanged; prior mod config restored with AllowRouteWrites=false;
  temporary run token/snapshot removed; no game/bootstrap process remained after shutdown.
  The game can generate ordinary autosaves while running; these were not asserted unchanged.
- Runtime tests are read-only discovery/inspection. Existing vehicles can report autopilot/fuel
  errors; a successful API inspection is not a declaration that their journeys are healthy.
  Dynamic assignment, actual transfer, home stop/reassignment and job save recovery remain unverified.

## References

Launch map/save options inspected against the community's actual launch-script implementation:
https://github.com/satisfactorymodding/Documentation/blob/master/modules/ROOT/pages/Development/TestingResources.adoc

## Generated rest-stop registration proof
`--rest-stop-building` verifies generated class identities, gives the custom schematic on the
prepared disposable snapshot, and checks recipe/building availability. It expects the pinned
KDataForge dependency and DataForge pack to be present. The agent tested temporary deployment
and removed both afterward. This flag does not claim placement or arrival/hold behavior.
The runner requires the explicit REST_STOP_BUILDING evidence line; command receipt alone fails.
Current host validation: 1716 core checks, 9 bridge checks, 26 rest-stop checks; 14 runner tests.

Persistent installation is now provided by `python3 scripts/install-filn-rest-stop.py`.
It keeps KDataForge and the pack installed; normal-session unlock no longer depends on a
snapshot test. Rest-stop verification also checks Build Gun producer recipe membership.
The user confirmed visible menu availability; road attachment/hold tests remain gated.

## Pinned user-built fixture
The default save is now the exact file in test-fixture.json (City_241026-150303.sav), with a
hash check before game shutdown. --save-file explicitly selects another save; --latest-save
explicitly opts back into newest-save selection. The fixture has three road-connected FILN
stations and a truck with a single DEPOT Station route. That is intentional Idle configuration;
native TooFewStations is expected there. The dispatcher must author the trip route itself.
The existing generic --route-bridge proof is not proof of this truck's full idle/trip/return lifecycle.

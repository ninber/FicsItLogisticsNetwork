# Verification evidence: native completion

2026-11-24. This is an incomplete implementation and is not ready for release.

- Baseline logs/first-build.log: 177/177, Result: Succeeded, 269.56 s. Original parent
shell exit code cannot be recovered retrospectively; it did not record PIPESTATUS.
- Fresh logs/incremental-verification-3.log and .exit: actual exit 0; 301/301, 386.37 s,
Result: Succeeded. Host dependencies patch/dos2unix reused from existing container layer
into toolchains/host-tools/bin. No new dependency downloads or cache deletion.
- logs/native-station-build.log/.exit: exit 0, UHT wrote 3 outputs, FILN C++ and shared
library linked; 36.49 s. Later full-load changes: logs/native-resource-build.log/.exit,
exit 0; 30.50 s. These are Linux editor modules, not Win64 game binaries.
- Installed StarterProject HEAD 0170e5feece4bebcb4baa2ec4806587300160285, SML 3.12.1,
game dependency >=520140, UE Build.version 5.6.1 / CL87 / ++5.6.1-CSS.
- logs/policy-tests.log and policy-tests-clang.log: 3245 core checks + 67 dispatch
checks PASS on GCC and Clang. Supplied snapshot policy only, no game API exercised.
- logs/wwise-verification.log: configured project XML, Windows Init.bnk BKHD header,
JSON metadata, Linux_x64 and x64_vc170 Profile/Release libraries PASS. SDK header
2025.1.6.9117; integration 2025.1.6.9117.4248. No Linux generated bank was found;
Win64 banks exist. No claims of actual audio playback or packaging from file checks.
- Original graphical Turnkey failure: AutomationTool ErrorLog.txt reports MSB3202
engine C# references through desktop symlink. Physical path invocation removes that
particular ambiguity; UAT default C# build still fails in this sandbox before logging.
Single-worker non-shared C# compilation passed in 88.34 s (15 warnings, 0 errors).
- logs/turnkey-precompiled.log/.exit: Win64 Status=Valid, BUILD SUCCESSFUL, actual
exit 0. headless.sh prepare-uat then -NoCompile reuses the compiled tooling. This
fixes Turnkey startup; it does not establish successful MSVC execution.
- SDK smoke probe FAIL: Wine unavailable in host execution environment. UBT detects
SDK 10.0.22621.0, but that does not establish compiler/linker functionality.
- Podman FAIL: cannot chmod /run/user/1000/libpod on read-only filesystem. Existing
container metadata identifies satisfactory-modding-nvidia with NVIDIA device mounts;
this is not an executed Vulkan proof. Detection script performs a real probe when
Podman access is available. No container recreated or started to test setup.
- GitHub shell DNS unavailable. Connector read PR #1 and files pinned to head
250fbb90533060438fbea5b6d94d9d00a26ff92b. Local Git source checkout now has a review branch feature/native-dispatch-policy
created from the clean local main; the remote commit ancestry was not fetched;
canonical working source imports those exact policy/docs files before extending them.
No false claim that remote branch history has been fetched or PR updated.
- User's existing StarterProject Config/DefaultGame.ini modification retained.
- No game process controlled, no input simulated, no save modified, no installed mod
or launch option changed. No graphical editor or Wwise Authoring launched.

## Failed acceptance / remaining implementation

Native automatic dispatch, route-cost cache, depot/refueling port, actual fluid quantity
verification, native management/resource UI, legacy importer, full uninstall and independent
load without FILN, save/restart/multiplayer integration and Win64 package are incomplete.
These are open tasks, not successful skipped tests. No SMM package has been generated.

## Minimal environment actions if this restricted session cannot enter Distrobox

Run from an ordinary host terminal, leaving the game running:

```
cd /var/home/ninber/migrationPlan/Dev-folders/D-Dev/SatisfactoryModding
bash scripts/detect-dev-container.sh
```

Expected: a container name only after real NVIDIA Vulkan + Wine + compiler verification.
Then enter that returned name with `distrobox enter NAME`, return to this directory and run
`bash scripts/doctor.sh` and `bash scripts/headless.sh turnkey`. Expect doctor exit 0,
SDK smoke PASS with retained compiler/PE/runtime evidence and Turnkey successful SDK
validation. This is needed because session sandboxing prevents Podman runtime writes;
installed tools cannot be functionally verified from directory existence alone.

- Spec Kit check-prerequisites.sh --json --require-tasks --include-tasks passed,
resolving specs/014-native-completion.

- logs/native-final-build.log: a warnings-as-errors range-loop copy failed compilation.
Corrected to const reference; logs/native-final-build-2.log/.exit: actual exit 0,
FILN native-only C++/link succeeded in 36.83 s against the final station logic.
- logs/uht-explicit.log: initial explicit UHT failed writing outside the workspace.
Added explicit log path; logs/uht-explicit-2.log/.exit: exit 0, UHT succeeded in
6.54 s. Generated outputs already current (0 written).
- logs/package-verifier-tests.log: four negative artifact tests PASS. Source ZIP,
missing DLL, fake DLL and traversal paths rejected. No fake DLL is treated as release.
- First package invocation conflicted with the ongoing final native build; UBT rejected
it without compiling a second target. The headless package/UHT entry points now share
the build lock; package was retried only after native build and explicit UHT completed.

- Actual Alpakit invocation logs/win64-package-2.log completed UHT for both Win64 EGS
and Steam targets (2488 generated outputs each), selected MSVC 14.38.33130 / SDK
10.0.22621.0 and entered 626 compile actions. It stalled: upstream wine-msvc wrapper
selects an empty Wine executable, then waits on output FIFOs when Wine is absent.
Cancelled this task's package process with SIGINT (tool session exit 130). No archive
produced; do not interpret this as MSVC, cooking, packaging or release success.
- headless.sh package now requires the actual SDK compile/link/PE smoke probe before
starting UAT. This makes the missing-Wine case fail immediately rather than hang.
- Expanded doctor run in logs/doctor-native.log/.exit failed (exit 1) in the host sandbox:
Wine/git-lfs/msiextract unavailable and hardware Vulkan probe cannot initialize here.
This does not contradict the user's working container; the container could not be entered.
- Evidence-only acceptance report logs/native-acceptance.log/.exit intentionally exits 1:
22/22 actual runtime/release gates unresolved. It is not an Unreal integration test suite.

- Existing graphical editor log independently records Vulkan DeviceName NVIDIA GeForce
RTX 3090 and NVIDIA driver 620.70 (Saved/Logs/FactoryGame.log lines 563, 571, 868).
Existing container metadata shows NVIDIA device bindings on satisfactory-modding-nvidia.
This supports the user's working-environment report; a fresh per-container probe remains
blocked by this session's read-only Podman runtime.
- Review checkout portable suite rerun: logs/review-checkout-tests.log, 3245 + 67 PASS.
No commits, pushes, PR mutation or live installation performed. Review branch was created
locally from main, with exact PR policy file content imported before the documented fixes.

- Final player-control recovery guard compiled and linked: logs/native-player-guard-build.log/.exit, actual exit 0, 27.71 s. No compiler/package jobs from this task remain running.

## Continuation evidence

See [continuation.md](continuation.md) for executed adapter builds, portable results,
headless native inventory failures and exact Distrobox/package blocker. All earlier logs
and acceptance failures are retained. No runtime/release gate is marked passed.

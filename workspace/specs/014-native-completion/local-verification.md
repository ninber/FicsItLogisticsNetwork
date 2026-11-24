# Local build and test verification of GitHub main

GitHub baseline: `9623afb` (merged implementation through `067d68f`).
Verification checkout: `workspace/github/FILN-verification`, branch
`verification/native-20261125`. The divergent local handoff `af024eb` remains
untouched on `feature/native-dispatch-policy`; it was not merged over the newer
implementation. Previous canonical sources are preserved at the location in
`logs/gh-verification-baseline.json`.

## Executed results

| Check | Actual result |
| --- | --- |
| Portable native suites, GCC | PASS: 14 suites, 7,846 checks |
| Portable native suites, Clang | PASS: 14 suites, 7,846 checks |
| Python regressions | PASS: 113 tests |
| JavaScript save-tool regressions | PASS: 27 tests |
| Linux native UHT / compilation / link | PASS, actual exit 0 |
| Explicit UHT | PASS, actual exit 0, 7.04 seconds |
| EGS + Steam Win64 compile/link, cook, stage, Alpakit archive | PASS, actual exit 0 |
| Archive CRC, metadata, versioned SML-only dependency, x64 PE and exclusions | PASS |
| Both .modules files and imported DLLs | PASS: BuildId=SML, corresponding DLLs present, no legacy loader imports |
| Headless FILN.NativeInventory.Solid / Fluid | FAIL: 0 succeeded, 2 failed; Resize(2) returned 0 in generated FactoryGame code |

The inventory assertions were retained. The SDK editor's generated implementation
cannot establish actual station ingress or inventory behavior. Its log also
reports missing Linux Wwise banks and unrelated upstream automation initialization
warnings. These are not a passing game integration run. No production game was
started/stopped, no player input was automated, and no save or installed game mod
was modified.

## Verification fixes only

- Reused direct-container headless/SDK helpers with bounded diagnostic failures,
  short Win64 paths and a scoped Wine cwd launcher. No Podman or nested Distrobox.
- Added a module-private CoreMinimal PCH. This fixes the mixed physical/alias
  engine includes from the old shared PCH without deleting existing caches.
- Renamed local Owner variables hiding AActor::Owner and one shadowed slot-capacity
  variable; no dispatch/recovery behavior changes.
- Renamed idle-policy `near` to `homeNearby` to avoid the Win32 keyword macro;
  idle regression compilation now defines that macro before including the header.
- Restored the existing dashboard unknown.svg fallback omitted from GitHub.
- Installed the missing container Node.js test interpreter; reused the existing
  parser dependencies through NODE_PATH. Native toolchains were not reinstalled.

## Preserved evidence and artifact

Development archive: `workspace/test-results/gh-native-9623afb-20261124T221741Z/FicsItLogisticsNetwork-Windows.zip`.
Receipt/source hashes, target import lists, logs and inventory report are in the
same directory. Archive SHA-256:
`9f9f3d8c3a8e90669afe3d5d54828c4bf58a581fa00f2eee739f3eae9d08370f`.
The directory is mirrored in the verification checkout's ignored test-results
folder so the acceptance evidence resolves from that checkout too.

This is a development artifact, not a gameplay-validated release. No publication,
installation, server support or completed gameplay acceptance is claimed. The
native-package gate passes; legacy Lua save migration is explicitly out of native
scope, and the other 20 in-scope runtime gates remain unverified.

## Direct continuation

From the outer development directory:

```bash
bash scripts/install-filn.sh
bash scripts/headless.sh native
bash scripts/headless.sh package
python3 scripts/verify-native-package.py workspace/StarterProject/Saved/ArchivedPlugins/FicsItLogisticsNetwork/FicsItLogisticsNetwork-Windows.zip
```

Continue actual gameplay checks only with a separately authorized disposable test
session and user-controlled visual/UI inspection under workspace/AGENTS.md.
Do not alter generated FactoryGame stubs or weaken assertions to manufacture a
passing inventory test. Verification changes are recorded on the local verification branch; nothing was
pushed. GitHub implementation source was not rewritten.

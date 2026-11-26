# FicsIt Logistics Network (FILN)

Automatic solid and fluid truck logistics for Satisfactory: providers, requesters and dedicated home depots.

**Experimental source project. No installable SMM release exists yet.** The current scope is [Spec Kit feature 014](workspace/specs/014-native-completion/spec.md); see the [consistency audit](workspace/specs/014-native-completion/consistency-audit.md).

- `workspace/mods/FicsItLogisticsNetwork`: canonical SML C++ native mod. Automatic dispatch, home ownership/overflow, transfer receipts and native dashboard are implemented in source but gameplay is unverified. Target is standalone solid/fluid logistics with a FILN-owned selector; the native third-party adapter and manifest/build dependency are removed in source; solid/fluid station and depot recipe authoring and medium-aware native adapter now exist in source, but standalone selector, authored/cooked assets and real fluid behavior remain release blockers.
- `workspace/experiments/FILNLua`: separate historical UE4SS/KDataForge experiment and behavior reference, not native runtime. Its dashboard frontend is reused by the native read-only service. Do not install Lua dispatcher alongside native tests.

Intended gameplay: build a dedicated depot and ordinary LOAD/UNLOAD stations, select one compatible resource at each requester, assign vehicles through normal depot route/autopilot controls. No management panel or manual registration. Statistics are web-only.

See [release requirements](workspace/RELEASE.md), [development setup](DEVELOPMENT.md) and [native build instructions](workspace/mods/FicsItLogisticsNetwork/docs/HEADLESS.md).

## Host checks

```bash
bash scripts/test-filn.sh
python3 -m unittest discover -s scripts/tests
python3 scripts/report-native-acceptance.py
```

The acceptance report must fail while requirements remain unresolved. Host tests do not prove runtime compatibility. Historical Lua tests: `bash scripts/test-filn-lua.sh`.

Game saves, runtime state, logs, local credentials, downloaded toolchains, upstream StarterProject and generated archives are excluded. Historical evidence may refer to local-only files. Never interrupt the running game, automate its UI, or migrate the original City save.

No new general-purpose license is granted for FILN-owned code in this revision; earlier grants and third-party rights remain unaffected. Game/engine content retains upstream ownership. Dashboard icons are generated locally from the installed game, not distributed. See [publication compliance](workspace/PUBLICATION.md). Smarter Truck Stations is a technical reference only; no redistribution license was identified.

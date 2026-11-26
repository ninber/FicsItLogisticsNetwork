# Headless native development

Run from the outer SatisfactoryModding directory, in the verified development container.
Do not run game deployment or copy-to-game options. Existing shader/DDC/Zen/build caches
are retained. These commands do not open the graphical editor.

```
source scripts/env.sh
bash scripts/doctor.sh
bash scripts/headless.sh test
bash scripts/install-filn.sh          # copies into StarterProject only
bash scripts/headless.sh build        # incremental editor build, includes UHT as needed
bash scripts/headless.sh native       # only FILN module; requires initial build
bash scripts/headless.sh uht          # explicit existing-manifest header generation
bash scripts/headless.sh prepare-uat  # cached C# dependencies, single worker; no GUI
bash scripts/headless.sh turnkey
bash scripts/headless.sh author-depot # currently blocked: root-module authoring/reload
# bash scripts/headless.sh package    # only after fixed assets and standalone scope
python3 scripts/verify-native-package.py PATH_TO_GENERATED_ZIP
```

`prepare-uat` uses the installed .NET 8.0.300 bundle, physical EngineDir and an existing
NuGet restore. It does not download replacements. If a fresh restore is necessary, report
its missing package rather than silently changing engine dependencies. Subsequent UAT uses
-NoCompile to reuse the built script assemblies. An Alpakit DLL must actually exist first.
UAT logs go to logs/uat. Build uses logs/ubt.log and a nonblocking advisory flock; existing
builds launched outside this wrapper must also be checked before invocation.

`package` first executes the real Windows SDK compile/link/run smoke probe. This
prevents an upstream Wine wrapper from hanging on FIFOs when Wine is unavailable.

`package` deliberately accepts no additional arguments, so copy/launch/deployment options
cannot accidentally reach Alpakit. It does not prove package acceptance until an archive
exists and is inspected. Source ZIPs and Linux editor .so files are not Win64 packages.

Native station commands, for an independently authorized disposable test world only:

- `filn.list` resolves actual native station/vehicle object paths.
- `filn.station STATION_OBJECT_PATH RESOURCE_DESCRIPTOR_CLASS_PATH` configures an empty,
  undocked, unreserved vanilla station. Wrong solid/fluid forms and conflicting existing
  restrictions are rejected. This is a developer diagnostic, not the required player selector; current source still rejects fluids.
- `filn.prepareUninstall` restores prior native slot restrictions only after complete
  preflight. Active jobs, docking, resize/filter conflicts block the operation. It preserves
  physical actors and inventory contents. Save afterward; independent loading without FILN
  has not been tested and must not be claimed to work yet.

Vanilla fluids use descriptor-based native inventory counts. The code queries native
per-slot capacity instead of assuming an item-stack-to-liters conversion. Pipe ingress,
fluid descriptor transitions and real quantity semantics remain acceptance failures until
isolated game evidence exists. No custom fluid truck is introduced.

## Continuation commands

On a host shell without Wine, `headless.sh package` / `sdk` and doctor now delegate
through `scripts/dev-exec.sh`. Container choice is based on executable clang, Wine and
NVIDIA Vulkan probes. No container is recreated. A Podman permission failure remains
an environment failure, rather than a claim that Wine is absent from the machine.

```
bash scripts/dev-exec.sh bash scripts/doctor.sh
bash scripts/headless.sh integration
python3 scripts/report-native-acceptance.py
```

`integration` starts only UnrealEditor-Cmd with NullRHI, no sound and native automation
cases; it does not load an active player save. A nonempty automation report with two
passing tests is required. Generated StarterProject FactoryGame functions are stub bodies,
so this command cannot establish true game inventory or dispatcher behavior. Do not
weaken Resize/filter assertions if the header implementation returns default values.

Runtime commands are developer-only; see the [native README](../README.md). Old filn.manage and filn.legacy commands and management UI were removed. Automatic player workflow is required. Do not deploy dependency-based draft.

Local Codex reports native Linux editor compilation/linkage after unpublished unity-build fixes; depot authoring and fresh reload failed. Reconcile local work before changing C++. SML native root registration is proposed, not verified. Development prerequisites and package verifier now require the FILN SML-only descriptor; no other gameplay mod source/assets required. Sync canonical source; remove stale third-party project enablement separately in a clean development checkout, not the running game.

Current evidence: [consistency audit](../../../specs/014-native-completion/consistency-audit.md).

## Station blueprint settings (R17)
After syncing source, build the editor module, run `headless.sh author-depot` (adds and verifies the SaveGame `FILNResource` property on the existing solid/fluid logistics actor Blueprints, including fresh editor reload), then package. A DLL-only replacement is insufficient. Old authored class paths remain unchanged. In-game acceptance: configure both modes and media inside the designer, save/reopen, place twice, independently change/clear and save/reload; inspect dispatch and inventory. No designer station may enroll in the active network. Previously saved blueprints which never contained the resource cannot recover it from this change alone.

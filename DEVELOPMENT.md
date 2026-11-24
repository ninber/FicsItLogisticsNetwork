# Satisfactory modding on Bazzite

FILN release preparation: see [workspace/RELEASE.md](workspace/RELEASE.md).
Run `python3 scripts/prepare-filn-release.py` to generate a clean source handoff
and readiness report. FILN is not yet an installable Mod Manager release.

## Current status

Spec Kit has been initialized in `workspace/` for Codex, using Bash scripts.
Open that folder in Codex to use `$speckit-constitution`, `$speckit-specify`,
`$speckit-plan`, and `$speckit-tasks`.

The starter project has now been downloaded, including ExampleMod under
`Mods/GameFeatures/ExampleMod`. The engine, MSVC, and Wwise are still pending.
During initial setup, this session's shell
cannot resolve GitHub, Podman cannot write its runtime directory, and the root
`.git`, `.agents`, and `.codex` directories are mounted read-only. The writable
`workspace/` folder avoids those root metadata restrictions.

## Install the container and official community starter

Run from this directory in a normal Bazzite terminal:

```bash
bash scripts/setup-bazzite.sh
distrobox enter satisfactory-modding
source scripts/env.sh
bash scripts/doctor.sh
```

The setup script creates a Fedora 44 Distrobox with build tools, Wine, MSVC
extraction tools and Git LFS, without layering packages onto Bazzite. It clones
the Satisfactory Modding team's upstream project into `workspace/StarterProject`,
including its `Mods/GameFeatures/ExampleMod` starter mod (older versions use
`Mods/ExampleMod`). This is the community's official
starter, not a Coffee Stain supported SDK. The checkout retains upstream history
and records its commit in `workspace/starter-revision.txt`. Re-running does not
advance the checkout to a newer version automatically.

## Custom Unreal Engine and remaining dependencies

An installer is available as `scripts/install-css-engine.sh`. In a normal terminal with
GitHub access, authenticate with `gh auth login` if needed, complete the Epic/Ficsit account
linking below, and list accessible releases with:

```bash
gh release list --repo satisfactorymodding/UnrealEngine --limit 10
```

Select the release matching this project's `5.6.1-CSS` engine association, then run:

```bash
bash scripts/install-css-engine.sh <matching-release-tag>
```

The script downloads non-debug Linux parts, checks sizes and published SHA-256 digests
when available, extracts to a staging directory and verifies the engine version before
installing. It retains archives and refuses to replace an existing installation. Its shell
syntax and argument handling have been checked; download/extraction remain untested because
this session cannot reach GitHub from the terminal. It does not install MSVC or Wwise.

Use the matching game/SML/engine versions from the documentation. The current
guide specifies CSS Unreal Engine 5.6.1, MSVC 17.8, Windows SDK 10.0.22621,
Wwise SDK 2025.1.6.9117 and integration 2025.1.6.4248. The scripts fetch upstream's
default branch; check compatibility with your installed game before building.

1. Link your GitHub account with Epic Games and accept the GitHub organization
   invitation: https://www.unrealengine.com/en-US/ue-on-github
2. Grant access to the modding engine through https://linker.ficsit.app/link
3. Download all non-debug `UnrealEngine-CSS-Editor-Linux.tar.zst.*` parts from
   https://github.com/satisfactorymodding/UnrealEngine/releases/latest into
   `downloads/engine/`. Extract into the local toolchain directory:

```bash
mkdir -p toolchains/UnrealEngineCSS
cat downloads/engine/UnrealEngine-CSS-Editor-Linux.tar.zst.* | zstd -d | tar -xf - -C toolchains/UnrealEngineCSS
```

Inside the Distrobox, install MSVC following the upstream license prompts:

```bash
source scripts/env.sh
git clone --branch ue-patches https://github.com/mircearoata/msvc-wine.git toolchains/msvc-wine
python3 toolchains/msvc-wine/vsdownload.py --dest "$UE_WINE_MSVC" --msvc-version 17.8 --sdk-version 10.0.22621 --channel release.ltsc.17.8
bash toolchains/msvc-wine/install.sh "$UE_WINE_MSVC"
```

Download the Linux `wwise-cli` executable from
https://github.com/mircearoata/wwise-cli/releases/latest and save it as
`toolchains/wwise-cli`. Then:

```bash
chmod +x toolchains/wwise-cli
toolchains/wwise-cli download --sdk-version 2025.1.6.9117 --filter Packages=SDK --filter DeploymentPlatforms=Windows_vc160 --filter DeploymentPlatforms=Windows_vc170 --filter DeploymentPlatforms=Linux --filter DeploymentPlatforms= --filter Packages=Authoring
toolchains/wwise-cli integrate-ue --integration-version 2025.1.6.4248 --project "$SATISFACTORY_PROJECT"
wine ~/.cache/wwise-cli/wwise/2025.1.6.9117/Authoring/x64/Release/bin/Wwise.exe
```

In Wwise, create `SatisfactoryModLoader_WwiseProject` in `workspace/StarterProject`
and generate SoundBanks for all platforms. Follow the upstream guide to assign
the generated bank paths in Unreal. These GUI steps are still required.

## Build and open

Run inside the Distrobox after installing the dependencies:

```bash
source scripts/env.sh
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" -projectfiles "-project=$SATISFACTORY_PROJECT" -game -VSCode
bash scripts/build.sh
bash scripts/editor.sh
```

Use the engine-bundled compiler/toolchain. The host Homebrew Clang alone is not
a replacement. Override `UE_ROOT` if the CSS engine is already installed elsewhere.
The editor launcher passes the project explicitly, so engine registration is not
required. Native editor compilation and Windows mod packaging are separate steps;
configure Alpakit in the editor for your game installation.

## References

- Linux setup: https://docs.ficsit.app/satisfactory-modding/latest/Development/Linux/LinuxSetup.html
- Starter project: https://github.com/satisfactorymodding/SatisfactoryModLoader
- Spec Kit: https://github.github.io/spec-kit/integrations/codex.html

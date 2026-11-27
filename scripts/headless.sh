#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
action=${1:-}; shift || true
# Always execute in this environment. Launch Codex inside the existing development
# container when its tools are needed; never enter a nested container implicitly.
mkdir -p "$SATISFACTORY_MODDING_ROOT/logs"
export uebp_LogFolder="$SATISFACTORY_MODDING_ROOT/logs/uat"
export uebp_FinalLogFolder="$uebp_LogFolder"
mkdir -p "$uebp_LogFolder"
uat="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
case "$action" in
  test) exec bash "$SATISFACTORY_MODDING_ROOT/scripts/test-filn.sh" "$@" ;;
  build) exec bash "$SATISFACTORY_MODDING_ROOT/scripts/build.sh" "$@" ;;
  native) exec bash "$SATISFACTORY_MODDING_ROOT/scripts/build.sh" -Module=FicsItLogisticsNetwork "$@" ;;
  uht)
    # Keep the lock in this foreground shell, not in spawned tools or daemons.
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    manifest="$(dirname "$SATISFACTORY_PROJECT")/Intermediate/Build/Linux/FactoryEditor/Development/FactoryEditor.uhtmanifest"
    [[ -f "$manifest" ]] || { echo 'Run headless.sh build once to create the UHT manifest' >&2; exit 1; }
    "$UE_ROOT/Engine/Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet" "$UE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" -Mode=UnrealHeaderTool "$SATISFACTORY_PROJECT" "$manifest" -WarningsAsErrors -installed "-Log=$SATISFACTORY_MODDING_ROOT/logs/uht.log" 9>&- ;;
  integration)
    # Keep the lock in this foreground shell, not in spawned tools or daemons.
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    report=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/native-integration.XXXXXX")
    printf 'Native integration evidence: %s\n' "$report"
    cache_args=()
    if [[ "${FILN_INTEGRATION_MEMORY_DDC:-0}" == 1 ]]; then
      cache_args=(-ddc=InstalledNoZenLocalFallback -DDC-ForceMemoryCache)
    fi
    "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$SATISFACTORY_PROJECT" -unattended -NullRHI -NoSound -nop4 -nosplash -RenderOffscreen "${cache_args[@]}" '-ExecCmds=Automation RunTests FILN.NativeInventory' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$report/editor.log" -stdout 9>&-
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/verify-automation-report.py" "$report/index.json" ;;
  author-depot)
    [[ $# == 0 ]] || { echo 'author-depot accepts no arguments' >&2; exit 2; }
    # Keep the lock in this foreground shell, not in spawned tools or daemons.
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo 'Native build already running' >&2; exit 75; }
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/check-native-prerequisites.py" "$SATISFACTORY_PROJECT"
    report=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/depot-author.XXXXXX")
    if [[ "${FILN_DEPOT_UPGRADE_NAMES:-0}" == 1 ]]; then
      depot_dir="$(dirname "$SATISFACTORY_PROJECT")/Mods/FicsItLogisticsNetwork/Content/Depot"
      if [[ -d "$depot_dir" ]]; then
        cp -a -- "$depot_dir" "$report/previous-depot-content"
        printf 'Preserved previous FILN depot content: %s\n' "$report/previous-depot-content"
      fi
    fi
    export FILN_DEPOT_AUTHOR_RECEIPT="$report/receipt.json"
    "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$SATISFACTORY_PROJECT" -run=pythonscript "-script=$SATISFACTORY_MODDING_ROOT/scripts/author-native-depot.py" -unattended -NullRHI -NoSound -nop4 -nosplash "-abslog=$report/editor.log" -stdout 9>&-
    [[ -s "$FILN_DEPOT_AUTHOR_RECEIPT" ]] || { echo "BLOCKED: no successful asset-authoring receipt; inspect $report/editor.log" >&2; exit 1; }
    first_receipt="$FILN_DEPOT_AUTHOR_RECEIPT"
    export FILN_DEPOT_AUTHOR_RECEIPT="$report/reload.json" FILN_DEPOT_VALIDATE_ONLY=1
    "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$SATISFACTORY_PROJECT" -run=pythonscript "-script=$SATISFACTORY_MODDING_ROOT/scripts/author-native-depot.py" -unattended -NullRHI -NoSound -nop4 -nosplash "-abslog=$report/reload.log" -stdout 9>&-
    [[ -s "$FILN_DEPOT_AUTHOR_RECEIPT" ]] && cmp -s "$first_receipt" "$FILN_DEPOT_AUTHOR_RECEIPT" || { echo "BLOCKED: fresh editor reload did not reproduce authored assets; inspect $report" >&2; exit 1; }
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/check-native-prerequisites.py" "$SATISFACTORY_PROJECT" --require-depot
    printf 'Station authoring and reload receipts: %s\n' "$report" ;;
  prepare-uat)
    exec "$UE_ROOT/Engine/Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet" build "$(dirname "$SATISFACTORY_PROJECT")/Mods/Alpakit/Source/Alpakit.Automation/Alpakit.Automation.csproj" --no-restore -m:1 -nr:false /p:UseSharedCompilation=false "/p:EngineDir=$UE_ROOT/Engine" "/p:EnginePath=$UE_ROOT/Engine" /p:Configuration=Development ;;
  sdk) exec bash "$SATISFACTORY_MODDING_ROOT/scripts/verify-windows-sdk.sh" "$@" ;;
  turnkey) exec "$uat" -NoCompile "-ScriptsForProject=$SATISFACTORY_PROJECT" Turnkey -command=VerifySdk -platform=Win64 "-project=$SATISFACTORY_PROJECT" -utf8output ;;
  package)
    [[ $# == 0 ]] || { echo 'Package accepts no overrides; deployment/launch is deliberately excluded' >&2; exit 2; }
    # Keep the lock in this foreground shell, not in spawned tools or daemons.
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    bash "$SATISFACTORY_MODDING_ROOT/scripts/verify-windows-sdk.sh"
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/check-native-prerequisites.py" "$SATISFACTORY_PROJECT" --require-depot
    # Quarantine previous outputs so a no-op/misrouted package cannot pass by
    # reusing an old ZIP. Preserve them for diagnostics, never touch game saves.
    project_dir=$(dirname -- "$SATISFACTORY_PROJECT")
    archive_dir="$project_dir/Saved/ArchivedPlugins/FicsItLogisticsNetwork"
    archive="$archive_dir/FicsItLogisticsNetwork-Windows.zip"
    descriptor="$project_dir/Mods/FicsItLogisticsNetwork/FicsItLogisticsNetwork.uplugin"
    canonical="$SATISFACTORY_MODDING_ROOT/workspace/mods/FicsItLogisticsNetwork/FicsItLogisticsNetwork.uplugin"
    cmp -s -- "$descriptor" "$canonical" || { echo 'BLOCKED: StarterProject descriptor differs from canonical source' >&2; exit 1; }
    report=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/package.XXXXXX")
    cp -- "$descriptor" "$report/expected.uplugin"
    if [[ -e "$archive_dir" || -L "$archive_dir" ]]; then
      mv -- "$archive_dir" "$report/previous-archive"
      printf 'Preserved prior package: %s\n' "$report/previous-archive"
    fi
    source "$SATISFACTORY_MODDING_ROOT/scripts/win64-paths.sh"
    uat="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
    "$uat" -NoCompile "-ScriptsForProject=$SATISFACTORY_PROJECT" PackagePlugin "-project=$SATISFACTORY_PROJECT" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=FicsItLogisticsNetwork -platform=Win64 -build -nocompileeditor -installed 9>&-
    [[ -s "$archive" ]] || { echo "BLOCKED: UAT did not produce expected archive: $archive" >&2; exit 1; }
    cmp -s -- "$descriptor" "$report/expected.uplugin" && cmp -s -- "$canonical" "$report/expected.uplugin" || { echo 'BLOCKED: source descriptor changed during packaging' >&2; exit 1; }
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/verify-native-package.py" "$archive" --expected-descriptor "$report/expected.uplugin"
    sha256sum -- "$archive" | tee "$report/archive.sha256"
    printf 'Verified package evidence: %s\n' "$report" ;;
  *) echo 'Usage: bash scripts/headless.sh {test|build|native|uht|integration|author-depot|prepare-uat|sdk|turnkey|package}' >&2; exit 2 ;;
esac

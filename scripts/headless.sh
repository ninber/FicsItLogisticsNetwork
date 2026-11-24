#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
action=${1:-}; shift || true
# Wine is a dependency of the target build, not necessarily of this host shell.
if [[ "$action" == package || "$action" == sdk ]]; then
  if ! command -v wine >/dev/null && ! command -v wine64 >/dev/null; then
    exec bash "$SATISFACTORY_MODDING_ROOT/scripts/dev-exec.sh" bash "$SATISFACTORY_MODDING_ROOT/scripts/headless.sh" "$action" "$@"
  fi
fi
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
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    manifest="$(dirname "$SATISFACTORY_PROJECT")/Intermediate/Build/Linux/FactoryEditor/Development/FactoryEditor.uhtmanifest"
    [[ -f "$manifest" ]] || { echo 'Run headless.sh build once to create the UHT manifest' >&2; exit 1; }
    exec "$UE_ROOT/Engine/Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet" "$UE_ROOT/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" -Mode=UnrealHeaderTool "$SATISFACTORY_PROJECT" "$manifest" -WarningsAsErrors -installed "-Log=$SATISFACTORY_MODDING_ROOT/logs/uht.log" ;;
  integration)
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    report=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/native-integration.XXXXXX")
    printf 'Native integration evidence: %s\n' "$report"
    cache_args=()
    if [[ "${FILN_INTEGRATION_MEMORY_DDC:-0}" == 1 ]]; then
      cache_args=(-ddc=InstalledNoZenLocalFallback -DDC-ForceMemoryCache)
    fi
    "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor-Cmd" "$SATISFACTORY_PROJECT" -unattended -NullRHI -NoSound -nop4 -nosplash -RenderOffscreen "${cache_args[@]}" '-ExecCmds=Automation RunTests FILN.NativeInventory' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report" "-abslog=$report/editor.log" -stdout
    python3 "$SATISFACTORY_MODDING_ROOT/scripts/verify-automation-report.py" "$report/index.json" ;;
  prepare-uat)
    exec "$UE_ROOT/Engine/Binaries/ThirdParty/DotNet/8.0.300/linux-x64/dotnet" build "$(dirname "$SATISFACTORY_PROJECT")/Mods/Alpakit/Source/Alpakit.Automation/Alpakit.Automation.csproj" --no-restore -m:1 -nr:false /p:UseSharedCompilation=false "/p:EngineDir=$UE_ROOT/Engine" "/p:EnginePath=$UE_ROOT/Engine" /p:Configuration=Development ;;
  sdk) exec bash "$SATISFACTORY_MODDING_ROOT/scripts/verify-windows-sdk.sh" "$@" ;;
  turnkey) exec "$uat" -NoCompile "-ScriptsForProject=$SATISFACTORY_PROJECT" Turnkey -command=VerifySdk -platform=Win64 "-project=$SATISFACTORY_PROJECT" -utf8output ;;
  package)
    exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
    flock -n 9 || { echo "Native build already running" >&2; exit 75; }
    [[ $# == 0 ]] || { echo 'Package accepts no overrides; deployment/launch is deliberately excluded' >&2; exit 2; }
    bash "$SATISFACTORY_MODDING_ROOT/scripts/verify-windows-sdk.sh"
    exec "$uat" -NoCompile "-ScriptsForProject=$SATISFACTORY_PROJECT" PackagePlugin "-project=$SATISFACTORY_PROJECT" -clientconfig=Shipping -serverconfig=Shipping -utf8output -DLCName=FicsItLogisticsNetwork -platform=Win64 -build -nocompileeditor -installed ;;
  *) echo 'Usage: bash scripts/headless.sh {test|build|native|uht|integration|prepare-uat|sdk|turnkey|package}' >&2; exit 2 ;;
esac

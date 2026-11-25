#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
[[ -x "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ]] || { echo 'Custom CSS engine missing; see README.md.' >&2; exit 1; }
[[ -f "$SATISFACTORY_PROJECT" ]] || { echo 'Starter project missing; run scripts/setup-bazzite.sh.' >&2; exit 1; }
python3 "$SATISFACTORY_MODDING_ROOT/scripts/check-native-prerequisites.py" "$SATISFACTORY_PROJECT"
mkdir -p "$SATISFACTORY_MODDING_ROOT/logs"
exec 9>"$SATISFACTORY_MODDING_ROOT/logs/native-build.lock"
flock -n 9 || { echo "Another FILN build owns the build lock" >&2; exit 75; }
 # Parent shell owns the lock; do not let compiler or daemon descendants inherit it.
"$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" FactoryEditor Linux Development "-Project=$SATISFACTORY_PROJECT" "-Log=$SATISFACTORY_MODDING_ROOT/logs/ubt.log" "$@" 9>&-

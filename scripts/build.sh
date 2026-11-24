#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
[[ -x "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" ]] || { echo 'Custom CSS engine missing; see README.md.' >&2; exit 1; }
[[ -f "$SATISFACTORY_PROJECT" ]] || { echo 'Starter project missing; run scripts/setup-bazzite.sh.' >&2; exit 1; }
exec "$UE_ROOT/Engine/Build/BatchFiles/Linux/Build.sh" FactoryEditor Linux Development "-Project=$SATISFACTORY_PROJECT" "$@"

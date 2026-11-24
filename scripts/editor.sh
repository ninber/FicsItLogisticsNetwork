#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
[[ -x "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" ]] || { echo "Custom CSS engine missing at $UE_ROOT; see README.md." >&2; exit 1; }
[[ -f "$SATISFACTORY_PROJECT" ]] || { echo 'Starter project missing; run scripts/setup-bazzite.sh.' >&2; exit 1; }
exec "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$SATISFACTORY_PROJECT" "$@"

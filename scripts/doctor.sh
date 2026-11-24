#!/usr/bin/env bash
set -uo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
missing=0
for tool in git git-lfs python3 msiextract tar zstd dos2unix wine clang make; do
  if command -v "$tool" >/dev/null; then printf 'OK      %s\n' "$tool"; else printf 'MISSING %s\n' "$tool"; missing=1; fi
done
example="$(dirname "$SATISFACTORY_PROJECT")/Mods/GameFeatures/ExampleMod/ExampleMod.uplugin"
[[ -f "$example" ]] || example="$(dirname "$SATISFACTORY_PROJECT")/Mods/ExampleMod/ExampleMod.uplugin"
for path in "$SATISFACTORY_PROJECT" "$example" "$UE_ROOT/Engine/Binaries/Linux/UnrealEditor" "$UE_WINE_MSVC" "$(dirname "$SATISFACTORY_PROJECT")/Plugins/Wwise/Wwise.uplugin"; do
  if [[ -e "$path" ]]; then printf 'OK      %s\n' "$path"; else printf 'MISSING %s\n' "$path"; missing=1; fi
done
exit "$missing"

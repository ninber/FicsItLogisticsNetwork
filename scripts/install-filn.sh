#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/workspace/mods/FicsItLogisticsNetwork"
target="$root/workspace/StarterProject/Mods/FicsItLogisticsNetwork"
[[ -f "$root/workspace/StarterProject/FactoryGame.uproject" ]] || { echo 'StarterProject missing' >&2; exit 1; }
mkdir -p "$target"
# Remove only retired canonical filenames from the generated StarterProject
# copy. Never remove arbitrary user-authored Content or editor assets.
for retired in Source/FicsItLogisticsNetwork/Private/FILNStationControls.cpp Source/FicsItLogisticsNetwork/Private/LogisticsMigration.cpp Source/FicsItLogisticsNetwork/Public/FILNLegacy.h tests/legacy_tests.cpp; do
  if [[ ! -e "$source_dir/$retired" && -f "$target/$retired" ]]; then
    rm -- "$target/$retired"
    printf 'Removed retired generated source: %s\n' "$retired"
  fi
done
# Copy changed files only. Preserving the canonical source timestamp on changed
# headers can leave UHT-generated code newer than its inputs after a repo sync.
# Do not invalidate incremental build caches on a no-op installation.
copy_changed() {
  local file=$1 relative destination
  relative=${file#"$source_dir/"}
  destination="$target/$relative"
  if [[ -f "$destination" && ! -L "$destination" ]] && cmp -s -- "$file" "$destination"; then
    return
  fi
  mkdir -p -- "$(dirname -- "$destination")"
  cp -a --no-preserve=timestamps -- "$file" "$destination"
}
for entry in FicsItLogisticsNetwork.uplugin Source Config tests docs README.md LICENSE.txt .gitignore; do
  if [[ -d "$source_dir/$entry" ]]; then
    find "$source_dir/$entry" \( -type f -o -type l \) -print0 |
      while IFS= read -r -d '' file; do copy_changed "$file"; done
  else
    copy_changed "$source_dir/$entry"
  fi
done
if [[ -d "$source_dir/Content" ]]; then
  # Merge committed authored assets only; do not delete local editor content.
  cp -an "$source_dir/Content" "$target/"
fi
printf 'Installed canonical FILN source into %s\nRe-run after source edits; edit workspace/mods/FicsItLogisticsNetwork as the source of truth.\n' "$target"

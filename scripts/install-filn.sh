#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/workspace/mods/FicsItLogisticsNetwork"
target="$root/workspace/StarterProject/Mods/FicsItLogisticsNetwork"
[[ -f "$root/workspace/StarterProject/FactoryGame.uproject" ]] || { echo 'StarterProject missing' >&2; exit 1; }
mkdir -p "$target"
# Remove only retired canonical filenames from the generated StarterProject
# copy. Never remove arbitrary user-authored Content or editor assets.
for retired in Source/FicsItLogisticsNetwork/Private/LogisticsMigration.cpp Source/FicsItLogisticsNetwork/Public/FILNLegacy.h tests/legacy_tests.cpp; do
  if [[ ! -e "$source_dir/$retired" && -f "$target/$retired" ]]; then
    rm -- "$target/$retired"
    printf 'Removed retired generated source: %s\n' "$retired"
  fi
done
# Copy canonical sources without deleting editor-generated files or assets.
for entry in FicsItLogisticsNetwork.uplugin Source Config tests docs README.md LICENSE.txt .gitignore; do
  cp -a "$source_dir/$entry" "$target/"
done
printf 'Installed canonical FILN source into %s\nRe-run after source edits; edit workspace/mods/FicsItLogisticsNetwork as the source of truth.\n' "$target"

#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
game_dir=${1:-$HOME/.local/share/Steam/steamapps/common/Satisfactory}
target="$game_dir/Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts"
if [[ ! -f "$target/main.lua" ]]; then
  target="$game_dir/FactoryGame/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts"
fi
[[ -f "$target/main.lua" ]] || { echo 'Installed FILNLua not found; pass the Satisfactory game directory as the first argument.' >&2; exit 1; }
backup=$(mktemp -d "$(dirname -- "$target")/Scripts-backup.XXXXXX")
cp -a "$target/." "$backup/"
# Copy every canonical module; preserve operator configuration and runtime state.
for module in "$root"/workspace/experiments/FILNLua/Scripts/*.lua; do
  [[ $(basename -- "$module") == Config.lua ]] && continue
  cp -- "$module" "$target/"
done
# Remove superseded editor code only after the complete installation backup above.
rm -f -- "$target/SettingsSchema.lua" "$target/SettingsStore.lua" "$target/SettingsService.lua" "$target/JsonView.lua" "$target/settings-view.json"
printf 'FILNLua updated. Configuration preserved. Backup: %s\nLoad the updated scripts on your next normal Satisfactory restart.\n' "$backup"

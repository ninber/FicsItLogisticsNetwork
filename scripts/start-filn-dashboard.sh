#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
native_dir="${1:-$HOME/.local/share/Steam/steamapps/compatdata/526870/pfx/drive_c/users/steamuser/AppData/Local/FactoryGame/Saved/FILN}"
[[ $# -le 1 && -d "$(dirname -- "$native_dir")" ]] || { echo 'BLOCKED: specify the actual game Saved/FILN directory' >&2; exit 1; }
# Manual diagnostic run only; no fallback to Lua or an unrelated installed unit.
# Automatic lifecycle is installed separately on the Bazzite host.
exec python3 "$root/scripts/filn-dashboard.py" --native-dir "$native_dir"

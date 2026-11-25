#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
native_dir="${1:-$HOME/.local/share/Steam/steamapps/compatdata/526870/pfx/drive_c/users/steamuser/AppData/Local/FactoryGame/Saved/FILN}"
[[ $# -le 1 && -d "$(dirname -- "$native_dir")" ]] || { echo 'BLOCKED: specify the actual game Saved/FILN directory' >&2; exit 1; }
python3 - "$root" "$native_dir" <<'PY'
from pathlib import Path
import subprocess, sys
root=Path(sys.argv[1]).resolve()
native_dir=Path(sys.argv[2]).resolve()
native_dir.mkdir(exist_ok=True)
sys.path.insert(0,str(root/'scripts'))
from filn_dashboard_lifecycle import native_service_units, native_world
unitdir=Path.home()/'.config/systemd/user'
unitdir.mkdir(parents=True,exist_ok=True)
unit,watch=native_service_units(root,native_dir)
# Only the path watch is enabled at login, never the HTTP server.
subprocess.run(['systemctl','--user','disable','filn-dashboard.service'],check=False)
subprocess.run(['systemctl','--user','stop','filn-dashboard.path','filn-dashboard.service'],check=False)
(unitdir/'filn-dashboard.service').write_text(unit)
(unitdir/'filn-dashboard.path').write_text(watch)
subprocess.run(['systemctl','--user','daemon-reload'],check=True)
subprocess.run(['systemctl','--user','enable','--now','filn-dashboard.path'],check=True)
if native_world(native_dir/'dashboard-native-mod.txt'):
    subprocess.run(['systemctl','--user','start','filn-dashboard.service'],check=True)
print('Native dashboard watch installed: '+str(native_dir))
print('HTTP starts on a fresh native world heartbeat, not on login or Lua activation.')
PY

#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 - "$root" <<'PY'
from pathlib import Path
import subprocess, sys
root=Path(sys.argv[1]).resolve()
unitdir=Path.home()/'.config/systemd/user'
unitdir.mkdir(parents=True,exist_ok=True)
heartbeat=Path.home()/'.local/share/Steam/steamapps/common/Satisfactory/Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts/dashboard-mod.txt'
def quote(path):
    return '"'+str(path).replace('\\','\\\\').replace('"','\\"').replace('%','%%')+'"'
unit='''[Unit]
Description=FILN local station-status dashboard (running game mod only)

[Service]
Type=simple
ExecStart=/usr/bin/python3 %s --with-game-mod
'''%quote(root/'scripts/filn-dashboard.py')
watch='''[Unit]
Description=Watch for FILN mod activation

[Path]
PathChanged=%s
Unit=filn-dashboard.service

[Install]
WantedBy=default.target
'''%str(heartbeat).replace('%','%%')
# The server has no Install section and must never be enabled at login.
subprocess.run(['systemctl','--user','disable','filn-dashboard.service'],check=False)
subprocess.run(['systemctl','--user','stop','filn-dashboard.path','filn-dashboard.service'],check=False)
(unitdir/'filn-dashboard.service').write_text(unit)
(unitdir/'filn-dashboard.path').write_text(watch)
subprocess.run(['systemctl','--user','daemon-reload'],check=True)
subprocess.run(['systemctl','--user','enable','--now','filn-dashboard.path'],check=True)
# Keep the user's current game usable while its old Lua module is still cached.
sys.path.insert(0,str(root/'scripts'))
from filn_dashboard_lifecycle import game_processes, fresh_heartbeat
if game_processes() or fresh_heartbeat(heartbeat):
    subprocess.run(['systemctl','--user','start','filn-dashboard.service'],check=True)
print('FILN dashboard activated by the mod heartbeat; server stops with the game/mod.')
print('Only a systemd file watch is enabled at login; the HTTP server is not.')
PY

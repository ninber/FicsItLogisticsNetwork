#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 1 ]]; then
  echo 'Usage: bash scripts/filn-lua-command.sh ping|scan|cargo_stations|inspect|pair|plan|apply|start|status|stop|reset|restore [V1 S1 ...]' >&2
  exit 2
fi
case "$1" in
  ping|scan|blueprints_reload|blueprints_organize|blueprints_verify|save_autosave|requests|recover_vehicle|depot_fuel|player_state|auto_start|auto_stop|auto_status|save_city|hub_research|hub_research_save|programmable_unlock|cargo_stations|inspect|rest_stop_fixtures|station|stations|autotest_prepare|autotest_rest_stop_building|autotest_rest_stop_hologram|autotest_route_bridge|pair|plan|apply|start|status|stop|reset|restore) ;;
  *) echo 'Unknown FILN command' >&2; exit 2 ;;
esac
for arg in "${@:2}"; do
  [[ $arg =~ ^[VS][1-9][0-9]*$ ]] || { echo 'Arguments must be scan IDs such as V1 or S2' >&2; exit 2; }
done
game_dir=${FILN_GAME_DIR:-$HOME/.local/share/Steam/steamapps/common/Satisfactory}
target="$game_dir/Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts"
if [[ ! -f "$target/main.lua" ]]; then target="$game_dir/FactoryGame/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts"; fi
[[ -f "$target/main.lua" ]] || { echo 'Installed FILNLua not found; set FILN_GAME_DIR.' >&2; exit 1; }
python3 - "$target" "$@" <<'PY'
import pathlib,sys,time
scripts=pathlib.Path(sys.argv[1])
command=' '.join(sys.argv[2:])
log=scripts.parents[2]/'UE4SS.log'
offset=log.stat().st_size if log.exists() else 0
with (scripts/'commands.txt').open('a',encoding='utf-8') as stream:
    stream.write(command+'\n')
print(f'Queued {command}. Log: {log}',flush=True)
if not log.exists():
    print('Loader log not found. Start the game, load the save, then send the command again.')
    sys.exit(0)
deadline=time.monotonic()+6
last_result=None
buffer=b''
while time.monotonic()<deadline:
    if log.stat().st_size<offset: offset=0
    with log.open('rb') as stream:
        stream.seek(offset); data=stream.read(262144); offset=stream.tell()
    buffer+=data
    while b'\n' in buffer:
        raw,buffer=buffer.split(b'\n',1)
        line=raw.decode('utf-8',errors='replace')
        if '[FILN-Lua]' in line:
            print(line[line.index('[FILN-Lua]'):].rstrip(),flush=True)
            last_result=time.monotonic()
    if last_result is not None and time.monotonic()-last_result>0.8: break
    time.sleep(0.1)
if last_result is None:
    print('No Lua reply within 6 seconds. Confirm the updated mod is running with a loaded save.')
PY

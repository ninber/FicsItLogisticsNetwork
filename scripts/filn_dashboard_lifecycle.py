"""Dashboard activation belongs to the running mod, never to a login event."""
from pathlib import Path
import time


def fresh_heartbeat(path, now=None):
    now = time.time() if now is None else now
    try:
        stamp = float(path.read_text()[:64])
        return -5 <= now-stamp < 30
    except (OSError, ValueError):
        return False


def game_processes(proc=Path('/proc')):
    result = {}
    for entry in proc.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            comm = (entry/'comm').read_text().strip().lower()
            args = (entry/'cmdline').read_bytes().decode(errors='replace').split('\0')
            if comm.startswith('factorygame') and any(a.replace('\\','/').rsplit('/',1)[-1].lower() in
                    {'factorygamesteam-win64-shipping.exe','factorygamesteam.exe'} for a in args):
                result[entry.name] = (entry/'stat').read_text().rpartition(')')[2].split()[19]
        except (OSError, IndexError):
            pass
    return result


class ModLifetime:
    def __init__(self, heartbeat, now=time.time, processes=game_processes):
        self.heartbeat, self.now, self.processes = heartbeat, now, processes
        self.saw_mod = fresh_heartbeat(heartbeat, now())
        # Migration only: an already-running game has the old cached Lua module.
        # Capture its process identities; never attach to a future game from this fallback.
        self.existing = processes() if not heartbeat.exists() else {}

    def alive(self):
        if fresh_heartbeat(self.heartbeat, self.now()):
            self.saw_mod = True
            return True
        if self.saw_mod:
            return False
        current = self.processes()
        return any(current.get(pid) == identity for pid, identity in self.existing.items())

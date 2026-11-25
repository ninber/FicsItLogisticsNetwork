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


# Native mode never falls back to a process or a legacy Lua heartbeat.
def native_world(path, now=None):
    now = time.time() if now is None else now
    try:
        stamp, world = path.read_text(encoding='utf-8').splitlines()
        if not -5 <= now-float(stamp) < 30:
            return None
        if not world or len(world) > 64 or not all(c in '0123456789abcdefABCDEF-' for c in world):
            return None
        return world
    except (OSError, ValueError, UnicodeError):
        return None


class NativeModLifetime:
    def __init__(self, heartbeat, now=time.time):
        self.heartbeat, self.now = heartbeat, now
        self.world = native_world(heartbeat, now())

    def alive(self):
        # Capture the initial world; a replacement world gets a new service activation.
        return self.world is not None and native_world(self.heartbeat, self.now()) == self.world


def native_service_units(root, native_dir):
    def quote(path):
        return '"' + str(path).replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%').replace('$', '$$') + '"'
    heartbeat = native_dir/'dashboard-native-mod.txt'
    if '\n' in str(heartbeat) or '\r' in str(heartbeat) or '\n' in str(root) or '\r' in str(root):
        raise ValueError('Invalid newline in service path')
    unit = '[Unit]\nDescription=FILN native read-only dashboard (active world only)\n\n[Service]\nType=simple\nExecStart=/usr/bin/python3 ' + quote(root/'scripts/filn-dashboard.py') + ' --native-dir ' + quote(native_dir) + ' --with-game-mod\n'
    watch = '[Unit]\nDescription=Watch native FILN world heartbeat\n\n[Path]\nPathChanged=' + str(heartbeat).replace('%', '%%') + '\nUnit=filn-dashboard.service\n\n[Install]\nWantedBy=default.target\n'
    return unit, watch

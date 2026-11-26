#!/usr/bin/env python3
"""Loopback FILN telemetry with explicit parked-truck registration."""
import argparse
import json
import math
import re
import secrets
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from filn_dashboard_model import analyze
from filn_dashboard_lifecycle import ModLifetime, NativeModLifetime, native_world
from filn_depot_registration import plan as registration_plan, queue as queue_registration

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT/'workspace/experiments/FILNLua/dashboard'
DEFAULT_SCRIPTS = Path.home()/'.local/share/Steam/steamapps/common/Satisfactory/Engine/Binaries/Win64/ue4ss/Mods/FILNLua/Scripts'

class State:
    def __init__(self, scripts, native=False, native_lifetime=None):
        self.scripts, self.history, self.snapshot = scripts, {}, None
        self.native = native
        self.native_lifetime = native_lifetime
        self.lock = threading.Lock()
        self.token = secrets.token_hex(24)

    def status(self):
        with self.lock:
            now = time.time()
            demand = self.scripts/'dashboard-demand.txt'
            if not demand.exists() or now-demand.stat().st_mtime >= 5:
                demand.write_text(str(int(now)))
            error = None
            try:
                data = json.loads((self.scripts/'telemetry.json').read_text())
                if data.get('schema_version') != 1 or not isinstance(data.get('stations'), list) or not isinstance(data.get('vehicles'), list):
                    raise ValueError('Unsupported telemetry schema')
                if self.native and data.get('runtime') != 'native':
                    raise ValueError('Native telemetry required; refusing Lua snapshot')
                if self.native:
                    stamp = data.get('exported_at')
                    if isinstance(stamp, bool) or not isinstance(stamp, (int, float)) or not math.isfinite(stamp) or not -5 <= now-stamp <= 45:
                        raise ValueError('Native telemetry stale or invalid timestamp; no current snapshot')
                    if self.native_lifetime and (not self.native_lifetime.alive() or data.get('world') != self.native_lifetime.world):
                        raise ValueError('Native telemetry belongs to a different or inactive world')
                    if not isinstance(data.get('dispatcher'), dict) or not isinstance(data.get('errors'), list):
                        raise ValueError('Native telemetry missing dispatcher or sampling-error evidence')
                self.snapshot = data
            except (OSError, ValueError, TypeError) as exc:
                error = str(exc)
            if self.native:
                if error is not None:
                    self.snapshot = None
                    self.history.clear()
                return dict(runtime='native', csrf_token=None, registration=dict(supported=False,candidates=[],result=None), server_time=now, ready=self.snapshot is not None and not self.snapshot['errors'], error=error or ('Native sampling incomplete' if self.snapshot and self.snapshot['errors'] else None), progress=None, data=analyze(self.snapshot,now,self.history) if self.snapshot else None)
            progress = None
            try:
                progress = json.loads((self.scripts/'telemetry-progress.json').read_text())
            except (OSError, ValueError):
                pass
            registration=dict(supported=bool(self.snapshot and self.snapshot.get('dispatcher',{}).get('registration_supported')),candidates=[],result=None)
            if self.snapshot:
                try:registration['candidates']=registration_plan(self.snapshot,now)
                except ValueError:pass
            try:registration['result']=json.loads((self.scripts/'depot-registration-result.json').read_text())
            except (OSError,ValueError):pass
            try:
                pending=(self.scripts/'depot-registration.tsv').read_text().splitlines()[0]
                if not registration['result'] or registration['result'].get('request_id')!=pending:registration['result']=dict(request_id=pending,status='queued',results=[])
            except (OSError,IndexError):pass
            return dict(csrf_token=self.token,registration=registration,server_time=now,ready=self.snapshot is not None,error=error,progress=progress,
                        data=analyze(self.snapshot,now,self.history) if self.snapshot else None)


def make_server(scripts, port=8765, native=False, native_lifetime=None):
    state = State(scripts,native,native_lifetime)
    icon_root = Path(scripts)/'cache'/'icons'
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            if state.native:
                self.send_error(405,'Native dashboard is read-only');return
            host=self.headers.get('Host','')
            allowed=(f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}',f'filn.localhost:{self.server.server_port}')
            if host not in allowed or self.headers.get('Origin')!='http://'+host or not secrets.compare_digest(self.headers.get('X-FILN-Token',''),state.token):
                self.send_error(403);return
            if self.path!='/api/register-depots':self.send_error(404);return
            try:
                if self.headers.get('Content-Type')!='application/json' or int(self.headers.get('Content-Length','0')) not in range(1,65):raise ValueError('Invalid request body')
                payload=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
                if payload!={'action':'register_depots'}:raise ValueError('Unsupported action')
                with state.lock:
                    snapshot=json.loads((state.scripts/'telemetry.json').read_text())
                    if not snapshot.get('dispatcher',{}).get('registration_supported'):raise ValueError('Restart with the updated mod to register trucks')
                    result=queue_registration(state.scripts,snapshot)
                body=json.dumps(result).encode();self.send_response(202)
                self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
            except (OSError,ValueError,KeyError,TypeError) as exc:self.send_error(409,str(exc))
        def do_GET(self):
            # Fixed loopback Host prevents DNS-rebinding sites reading city telemetry.
            if self.headers.get('Host') not in (f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}', f'filn.localhost:{self.server.server_port}'):
                self.send_error(403); return
            path = urlsplit(self.path).path
            if path == '/api/status':
                try:
                    body = json.dumps(state.status(), ensure_ascii=False, allow_nan=False).encode()
                    content_type = 'application/json; charset=utf-8'
                except (OSError, ValueError, TypeError, KeyError) as exc:
                    self.send_error(503, str(exc)); return
            elif path == '/icons/unknown.svg':
                body = (ASSETS/'icons'/'unknown.svg').read_bytes()
                content_type = 'image/svg+xml'
            elif native and re.fullmatch(r'/icons/[0-9a-f]{40}/[0-9a-f]{40}\.png', path):
                revision, filename = path.split('/')[2:]
                image = icon_root/revision/filename
                try:
                    # Never follow a symlink out of the runtime-only cache.
                    if any(p.is_symlink() for p in (icon_root.parent, icon_root, image.parent, image)) or not image.is_file() or image.stat().st_size > 65536:
                        raise OSError('Icon unavailable')
                    body = image.read_bytes()
                    if not body.startswith(b'\x89PNG\r\n\x1a\n'):
                        raise OSError('Invalid icon')
                except OSError:
                    self.send_error(404); return
                content_type = 'image/png'
            elif path == '/health':
                body, content_type = json.dumps({'service':'filn-dashboard','version':1,'runtime':'native' if state.native else 'lua'}).encode(), 'application/json'
            elif path in ('/', '/app.js', '/style.css'):
                name = 'index.html' if path == '/' else path[1:]
                body = (ASSETS/name).read_bytes()
                content_type = {'index.html':'text/html; charset=utf-8','app.js':'text/javascript; charset=utf-8','style.css':'text/css; charset=utf-8'}[name]
            else:
                self.send_error(404); return
            self.send_response(200)
            self.send_header('Content-Type', content_type)
            self.send_header('Cache-Control','private, max-age=86400, immutable' if content_type=='image/png' else 'no-store')
            self.send_header('X-Content-Type-Options','nosniff')
            self.send_header('Content-Security-Policy',"default-src 'self'; frame-ancestors 'none'; object-src 'none'; base-uri 'none'")
            self.send_header('Content-Length',str(len(body)))
            self.end_headers();self.wfile.write(body)
        def log_message(self,*args):
            pass
    server = ThreadingHTTPServer(('127.0.0.1',port),Handler)
    server.daemon_threads = True
    server.state = state
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scripts-dir',type=Path,default=DEFAULT_SCRIPTS)
    parser.add_argument('--native-dir',type=Path,help='Native game Saved/FILN directory; read-only browser dashboard')
    parser.add_argument('--with-game-mod',action='store_true',help='Exit when the running mod/game stops')
    args = parser.parse_args()
    native = args.native_dir is not None
    if native:
        args.scripts_dir=args.native_dir.resolve()
        args.scripts_dir.mkdir(parents=True,exist_ok=True)
    elif not (args.scripts_dir/'main.lua').is_file():
        parser.error('Installed FILNLua Scripts directory not found')
    lifetime = (NativeModLifetime(args.scripts_dir/'dashboard-native-mod.txt') if native else ModLifetime(args.scripts_dir/'dashboard-mod.txt')) if args.with_game_mod else None
    if lifetime and not lifetime.alive():
        print('FILN mod is not running; dashboard remains stopped.',flush=True)
        return
    server = make_server(args.scripts_dir,native=native,native_lifetime=lifetime if native else None)
    def watch_mod():
        while lifetime.alive():
            time.sleep(3)
        print('FILN mod stopped; shutting down dashboard.',flush=True)
        server.shutdown()
    if lifetime:
        threading.Thread(target=watch_mod,daemon=True).start()
    def heartbeat():
        while True:
            (args.scripts_dir/'dashboard-server.txt').write_text(str(int(time.time())))
            time.sleep(5)
    if not native: threading.Thread(target=heartbeat,daemon=True).start()
    print('FILN dashboard: http://filn.localhost:8765/ — open manually in Steam overlay browser',flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()

if __name__ == '__main__':
    main()

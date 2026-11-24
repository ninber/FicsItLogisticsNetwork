#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if python3 - <<'PY'
import json, urllib.request
try:
    with urllib.request.urlopen('http://127.0.0.1:8765/health',timeout=2) as r:
        assert json.load(r)['service']=='filn-dashboard'
except Exception:
    raise SystemExit(1)
PY
then
  echo 'FILN dashboard already running: http://filn.localhost:8765/'
  exit 0
fi
mkdir -p "$root/workspace/test-results/dashboard"
if command -v systemd-run >/dev/null && systemctl --user show-environment >/dev/null 2>&1; then
  if [[ -f "$HOME/.config/systemd/user/filn-dashboard.service" ]]; then
    systemctl --user restart filn-dashboard.service
  else
  # Fallback for installations without the persistent login service.
  systemctl --user stop filn-dashboard.service 2>/dev/null || true
  systemd-run --user --unit=filn-dashboard --collect \
    --property=Restart=on-failure --property=WorkingDirectory="$root" \
    --property=StandardOutput="append:$root/workspace/test-results/dashboard/server.log" \
    --property=StandardError="append:$root/workspace/test-results/dashboard/server.log" \
    python3 "$root/scripts/filn-dashboard.py" "$@"
  fi
  sleep 1
  systemctl --user is-active --quiet filn-dashboard.service || { cat "$root/workspace/test-results/dashboard/server.log" >&2; exit 1; }
  systemctl --user show filn-dashboard.service --property=MainPID --value >"$root/workspace/test-results/dashboard/server.pid"
else
  nohup python3 "$root/scripts/filn-dashboard.py" "$@" >"$root/workspace/test-results/dashboard/server.log" 2>&1 </dev/null &
  pid=$!
  printf '%s\n' "$pid" >"$root/workspace/test-results/dashboard/server.pid"
  sleep 1
  kill -0 "$pid" 2>/dev/null || { cat "$root/workspace/test-results/dashboard/server.log" >&2; exit 1; }
fi
echo 'FILN dashboard running: http://filn.localhost:8765/ — open manually in Steam overlay browser.'

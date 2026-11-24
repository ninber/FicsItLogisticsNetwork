#!/usr/bin/env bash
# Execute the supplied argv in the existing, verified development environment.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
[[ $# -gt 0 ]] || { echo 'Usage: bash scripts/dev-exec.sh COMMAND [ARG ...]' >&2; exit 2; }
if [[ -f /run/.containerenv || -f /.dockerenv ]]; then
  bash "$root/scripts/probe-dev-container.sh" >&2
  cd -- "$root"
  exec "$@"
fi
command -v distrobox >/dev/null || { echo 'Distrobox is unavailable in this execution environment' >&2; exit 69; }
container=$(bash "$root/scripts/detect-dev-container.sh") || {
  result=$?
  printf 'Cannot verify existing development container (exit %s). Host=%s project=%s\n' "$result" "$(hostname)" "$root" >&2
  echo 'No container was created or toolchain reinstalled.' >&2
  exit "$result"
}
printf 'FILN execution environment: Distrobox %s; project %s\n' "$container" "$root" >&2
exec distrobox enter --name "$container" -- bash -lc 'cd -- "$1"; shift; exec "$@"' filn-dev "$root" "$@"

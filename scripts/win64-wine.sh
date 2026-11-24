#!/usr/bin/env bash
# Preserve the short logical cwd when UBT starts Wine from the same physical dir.
set -euo pipefail
[[ -n "${FILN_REAL_WINE_COMMAND:-}" ]] || { echo 'Missing real Wine command for Win64 launcher' >&2; exit 69; }
if [[ -n "${FILN_WIN64_ENGINE_SOURCE:-}" && -d "$FILN_WIN64_ENGINE_SOURCE" && . -ef "$FILN_WIN64_ENGINE_SOURCE" ]]; then
  cd -- "$FILN_WIN64_ENGINE_SOURCE"
fi
exec "$FILN_REAL_WINE_COMMAND" "$@"

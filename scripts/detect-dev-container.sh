#!/usr/bin/env bash
# Read-only detection. Never starts a stopped container.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
containers=$(podman ps --filter label=manager=distrobox --format '{{.Names}}')
[[ -n "$containers" ]] || { echo 'No running Distrobox containers found' >&2; exit 1; }
while IFS= read -r name; do
  [[ -z "${FILN_DEV_CONTAINER:-}" || "$name" == "$FILN_DEV_CONTAINER" ]] || continue
  if result=$(podman exec "$name" bash -lc 'exec bash "$1"' filn-probe "$root/scripts/probe-dev-container.sh" 2>&1); then
    if [[ "$result" =~ deviceName[[:space:]]*=[[:space:]]*NVIDIA && "$result" == *DISCRETE_GPU* ]]; then
      printf '%s\n' "$name"
      exit 0
    fi
  fi
  printf 'Rejected container %s: NVIDIA Vulkan/compiler/Wine probe failed\n' "$name" >&2
done <<< "$containers"
echo 'No running compatible NVIDIA Vulkan development container verified' >&2
exit 1

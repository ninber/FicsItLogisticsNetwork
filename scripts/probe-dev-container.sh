#!/usr/bin/env bash
# Functional probe only; no package installs, container starts or editor launch.
set -euo pipefail
clang --version
wine_command=$(command -v wine64 || command -v wine || true)
[[ -n "$wine_command" ]] || { echo 'No Wine executable in this environment' >&2; exit 69; }
"$wine_command" --version
rendering=$(env -u DISPLAY -u WAYLAND_DISPLAY vulkaninfo --summary 2>&1) || { printf '%s\n' "$rendering" >&2; exit 1; }
printf '%s\n' "$rendering"
# An NVIDIA loader error plus a different GPU must not be mistaken for NVIDIA support.
[[ "$rendering" =~ deviceName[[:space:]]*=[[:space:]]*NVIDIA && "$rendering" == *DISCRETE_GPU* ]] || {
  echo 'No enumerated NVIDIA discrete Vulkan device; software-only rendering is insufficient' >&2
  exit 1
}

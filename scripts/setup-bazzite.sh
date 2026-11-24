#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
name=satisfactory-modding
command -v distrobox >/dev/null
command -v podman >/dev/null
if ! podman container exists "$name"; then
  distrobox create --yes --name "$name" --image registry.fedoraproject.org/fedora:44 \
    --additional-packages 'git git-lfs python3 python3-pip msitools tar zstd dos2unix wine clang lld cmake ninja-build make gcc-c++ patch unzip curl vulkan-loader mesa-libGL libXcursor libXi libXrandr libxkbcommon-x11 alsa-lib'
fi
distrobox enter --name "$name" -- bash "$root/scripts/fetch-starter.sh"
printf '\nEnter the development environment with: distrobox enter %s\n' "$name"

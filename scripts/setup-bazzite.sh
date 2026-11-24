#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)
command -v distrobox >/dev/null
command -v podman >/dev/null
if name=$(bash "$root/scripts/detect-dev-container.sh"); then
  printf 'Reusing verified NVIDIA development container: %s\n' "$name"
else
  # Do not create a duplicate merely because the probe or Podman access failed.
  [[ "${1:-}" == --create ]] || { echo 'Detection failed. Explicit --create permits a new NVIDIA-enabled container.' >&2; exit 1; }
  name=${FILN_DEV_CONTAINER:-satisfactory-modding-nvidia}
  if podman container exists "$name"; then
    echo "Existing container $name was not verified. It has been preserved." >&2; exit 1
  fi
  distrobox create --yes --nvidia --name "$name" --image registry.fedoraproject.org/fedora:44 \
    --additional-packages 'git git-lfs gh xdg-utils ripgrep file python3 python3-pip msitools tar zstd dos2unix wine clang lld cmake ninja-build make gcc-c++ patch unzip curl vulkan-tools vulkan-loader mesa-libGL libXcursor libXi libXrandr libxkbcommon-x11 alsa-lib'
fi
# Existing compatible containers are preserved; install only missing utility packages.
distrobox enter --name "$name" -- bash -lc 'rpm -q gh xdg-utils ripgrep file >/dev/null 2>&1 || sudo dnf install -y gh xdg-utils ripgrep file'
distrobox enter --name "$name" -- bash "$root/scripts/fetch-starter.sh"
printf '\nEnter the development environment with: distrobox enter %s\n' "$name"

#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
project="$root/workspace/StarterProject"
repo=https://github.com/satisfactorymodding/SatisfactoryModLoader.git
command -v git-lfs >/dev/null || { echo 'git-lfs missing: run scripts/setup-bazzite.sh first.' >&2; exit 1; }
if [[ ! -d "$project" ]]; then
  git clone --recurse-submodules "$repo" "$project"
fi
[[ -f "$project/.git/config" ]] || { echo "Not a checkout: $project" >&2; exit 1; }
[[ $(git -C "$project" remote get-url origin) == "$repo" ]] || { echo 'Unexpected starter repository; stopping.' >&2; exit 1; }
git -C "$project" lfs install --local
git -C "$project" submodule update --init --recursive
git -C "$project" lfs pull
[[ -f "$project/FactoryGame.uproject" ]]
[[ -f "$project/Mods/GameFeatures/ExampleMod/ExampleMod.uplugin" || -f "$project/Mods/ExampleMod/ExampleMod.uplugin" ]]
git -C "$project" rev-parse HEAD > "$root/workspace/starter-revision.txt"
printf 'Starter project and upstream ExampleMod ready: %s\n' "$project"

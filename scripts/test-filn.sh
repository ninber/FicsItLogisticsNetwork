#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
plugin="$root/workspace/mods/FicsItLogisticsNetwork"
build_dir=$(mktemp -d /tmp/filn-tests.XXXXXX)
trap 'rm -rf -- "$build_dir"' EXIT
shopt -s nullglob
tests=("$plugin"/tests/*_tests.cpp)
(( ${#tests[@]} > 0 )) || { echo "No portable tests found" >&2; exit 1; }
for source in "${tests[@]}"; do
 name=$(basename "$source" .cpp)
 "${CXX:-g++}" -std=c++20 -Wall -Wextra -Wpedantic -Werror -O2 \
  -I"$plugin/Source/FicsItLogisticsNetwork/Public" "$source" -o "$build_dir/$name"
 "$build_dir/$name"
done

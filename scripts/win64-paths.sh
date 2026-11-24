#!/usr/bin/env bash
# Source after env.sh. Short logical paths keep MSVC below its Windows path limit.
# Targets remain the existing engine/project; no copying, cache deletion or container entry.
filn_alias_key=$(printf '%s\n%s' "$UE_ROOT" "$SATISFACTORY_PROJECT" | sha256sum)
filn_alias_root="/tmp/filn-win64-${UID}-${filn_alias_key:0:10}"
if [[ ! -e "$filn_alias_root" && ! -L "$filn_alias_root" ]]; then
  mkdir -m 700 -- "$filn_alias_root"
fi
[[ -d "$filn_alias_root" && ! -L "$filn_alias_root" && -O "$filn_alias_root" ]] || {
  echo "Refusing unsafe Win64 alias directory: $filn_alias_root" >&2; exit 1;
}
filn_link_alias() {
  local target alias resolved
  target=$1; alias=$2
  resolved=$(realpath -e -- "$target") || return
  if [[ -e "$alias" || -L "$alias" ]]; then
    [[ -L "$alias" && "$(readlink -f -- "$alias")" == "$resolved" ]] || {
      echo "Refusing to replace existing Win64 alias: $alias" >&2; return 1;
    }
  else
    ln -s -- "$resolved" "$alias"
  fi
}
filn_link_alias "$UE_ROOT" "$filn_alias_root/e"
filn_link_alias "$(dirname -- "$SATISFACTORY_PROJECT")" "$filn_alias_root/p"
export UE_ROOT="$filn_alias_root/e"
export SATISFACTORY_PROJECT="$filn_alias_root/p/$(basename -- "$SATISFACTORY_PROJECT")"
export FILN_REAL_WINE_COMMAND="${FILN_REAL_WINE_COMMAND:-$(command -v wine64 || command -v wine || true)}"
[[ -n "$FILN_REAL_WINE_COMMAND" ]] || { echo 'Wine unavailable for Win64 launcher' >&2; exit 69; }
export FILN_WIN64_ENGINE_SOURCE="$UE_ROOT/Engine/Source"
# UBT can canonicalize an action's cwd. Wine's initial DOS cwd must still use
# the short logical path so relative engine import-library names stay usable.
[[ ! -L "$filn_alias_root/bin" ]] || { echo 'Refusing symlinked Win64 launcher directory' >&2; exit 1; }
mkdir -p -- "$filn_alias_root/bin"
filn_link_alias "$(dirname -- "${BASH_SOURCE[0]}")/win64-wine.sh" "$filn_alias_root/bin/wine64"
export PATH="$filn_alias_root/bin:$PATH"
printf 'Win64 short paths: engine=%s project=%s\n' "$UE_ROOT" "$SATISFACTORY_PROJECT"
unset -f filn_link_alias
unset filn_alias_key filn_alias_root

#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
wine_command=$(command -v wine64 || command -v wine || true)
[[ -n "$wine_command" ]] || { echo 'BLOCKED: Wine unavailable in this execution environment. Run directly inside the existing development container; no container entry attempted.' >&2; exit 69; }
[[ -x "$UE_WINE_MSVC/bin/x64/cl" ]] || { echo 'MSVC wrapper missing' >&2; exit 1; }
build_dir=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/sdk-smoke.XXXXXX")
# Keep source, compiler output and executable as verification evidence.
cat > "$build_dir/sdk-smoke.cpp" <<'CPP'
#include <windows.h>
#include <cstdio>
int main() { SYSTEM_INFO info{}; GetNativeSystemInfo(&info); std::puts("FILN_WINDOWS_SDK_OK"); return info.dwNumberOfProcessors ? 0 : 1; }
CPP
# Raw output avoids the upstream wrapper's FIFO wait when Wine fails before
# opening its output pipes (for example, sandbox-denied wineserver sockets).
if WINE_MSVC_RAW_STDOUT=1 timeout -k 5 60 "$UE_WINE_MSVC/bin/x64/cl" /EHsc "$build_dir/sdk-smoke.cpp" "/Fo$build_dir/sdk-smoke.obj" "/Fe$build_dir/sdk-smoke.exe" /link kernel32.lib > "$build_dir/compiler.log" 2>&1; then
  :
else
  result=$?
  cat "$build_dir/compiler.log" >&2
  printf 'BLOCKED: SDK compiler check exited %s; evidence %s\n' "$result" "$build_dir" >&2
  exit "$result"
fi
file "$build_dir/sdk-smoke.exe" | tee "$build_dir/artifact.log"
if timeout -k 5 30 "$wine_command" "$build_dir/sdk-smoke.exe" > "$build_dir/runtime.log" 2>&1; then
  :
else
  result=$?
  cat "$build_dir/runtime.log" >&2
  printf 'BLOCKED: SDK executable check exited %s; evidence %s\n' "$result" "$build_dir" >&2
  exit "$result"
fi
rg -q '^FILN_WINDOWS_SDK_OK' "$build_dir/runtime.log"
printf 'PASS: MSVC, Windows SDK headers, CRT, kernel32 linking and PE execution; evidence %s\n' "$build_dir"

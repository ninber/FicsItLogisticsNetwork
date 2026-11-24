#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
wine_command=$(command -v wine64 || command -v wine || true)
[[ -n "$wine_command" ]] || { echo 'Wine unavailable in this execution environment' >&2; exit 1; }
[[ -x "$UE_WINE_MSVC/bin/x64/cl" ]] || { echo 'MSVC wrapper missing' >&2; exit 1; }
build_dir=$(mktemp -d "$SATISFACTORY_MODDING_ROOT/logs/sdk-smoke.XXXXXX")
# Keep source, compiler output and executable as verification evidence.
cat > "$build_dir/sdk-smoke.cpp" <<'CPP'
#include <windows.h>
#include <cstdio>
int main() { SYSTEM_INFO info{}; GetNativeSystemInfo(&info); std::puts("FILN_WINDOWS_SDK_OK"); return info.dwNumberOfProcessors ? 0 : 1; }
CPP
"$UE_WINE_MSVC/bin/x64/cl" /EHsc "$build_dir/sdk-smoke.cpp" "/Fo$build_dir/sdk-smoke.obj" "/Fe$build_dir/sdk-smoke.exe" /link kernel32.lib > "$build_dir/compiler.log" 2>&1
file "$build_dir/sdk-smoke.exe" | tee "$build_dir/artifact.log"
"$wine_command" "$build_dir/sdk-smoke.exe" > "$build_dir/runtime.log" 2>&1
rg -q '^FILN_WINDOWS_SDK_OK' "$build_dir/runtime.log"
printf 'PASS: MSVC, Windows SDK headers, CRT, kernel32 linking and PE execution; evidence %s\n' "$build_dir"

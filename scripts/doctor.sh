#!/usr/bin/env bash
set -uo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
if [[ "${FILN_DOCTOR_LOCAL:-0}" != 1 ]] && ! command -v wine >/dev/null && ! command -v wine64 >/dev/null; then
  exec bash "$SATISFACTORY_MODDING_ROOT/scripts/dev-exec.sh" bash "$SATISFACTORY_MODDING_ROOT/scripts/doctor.sh"
fi
missing=0
for tool in git git-lfs gh python3 msiextract tar zstd dos2unix wine clang make patch xdg-open xdg-mime vulkaninfo flock rg file; do
  if command -v "$tool" >/dev/null; then printf 'OK      %s\n' "$tool"; else printf 'MISSING %s\n' "$tool"; missing=1; fi
done
python3 - <<'PY' || missing=1
import json, os, subprocess
from pathlib import Path
root = Path(os.environ['SATISFACTORY_PROJECT']).parent
for path in [Path(os.environ['UE_ROOT']) / 'Engine/Build/Build.version', root / 'Mods/SML/SML.uplugin']:
    data = json.loads(path.read_text())
    print(path, {k: data[k] for k in ['MajorVersion','MinorVersion','PatchVersion','Changelist','VersionName','GameVersion'] if k in data})
print('StarterProject revision:', subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'], text=True).strip())
PY
python3 "$SATISFACTORY_MODDING_ROOT/scripts/verify-wwise.py" || missing=1
bash "$SATISFACTORY_MODDING_ROOT/scripts/verify-windows-sdk.sh" || missing=1
if rendering=$(env -u DISPLAY -u WAYLAND_DISPLAY vulkaninfo --summary 2>&1); then
  printf '%s\n' "$rendering"
  if [[ "$rendering" != *DISCRETE_GPU* && "$rendering" != *INTEGRATED_GPU* ]]; then
    echo 'FAIL    No hardware Vulkan device; software-only rendering is insufficient'; missing=1
  fi
else
  printf 'FAIL    Vulkan probe: %s\n' "$rendering"; missing=1
fi
exit "$missing"

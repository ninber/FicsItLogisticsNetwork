#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $# != 1 || $1 == -* || ! $1 =~ ^[A-Za-z0-9._+-]+$ ]]; then
  echo 'Usage: bash scripts/install-css-engine.sh <matching-release-tag>' >&2
  echo 'Choose the CSS 5.6.1 release matching the starter project from:' >&2
  echo 'https://github.com/satisfactorymodding/UnrealEngine/releases' >&2
  exit 2
fi
tag=$1
repo=satisfactorymodding/UnrealEngine
target="$root/toolchains/UnrealEngineCSS"
download="$root/downloads/engine/$tag"
for tool in gh python3 zstd tar; do command -v "$tool" >/dev/null || { echo "Missing tool: $tool" >&2; exit 1; }; done
[[ ! -e "$target" ]] || { echo "Target already exists; inspect before replacing: $target" >&2; exit 1; }
echo "Checking authenticated access to $repo, release $tag..."
metadata=$(gh api "repos/$repo/releases/tags/$tag") || {
  echo 'Cannot read release. Check connectivity and gh auth status. For a 404, verify the tag and complete Epic/GitHub linking plus https://linker.ficsit.app/link .' >&2
  exit 1
}
mkdir -p "$download" "$root/toolchains"
printf '%s\n' "$metadata" > "$download/release.json"
python3 - "$download" <<'PY'
import json, pathlib, re, sys
p=pathlib.Path(sys.argv[1])
release=json.loads((p/'release.json').read_text())
assets=sorted((a for a in release['assets'] if re.fullmatch(r'UnrealEngine-CSS-Editor-Linux\.tar\.zst\.[A-Za-z0-9]+',a['name'])),key=lambda a:a['name'])
if not assets: raise SystemExit('No non-debug Linux archive parts in this release; stopping.')
(p/'parts.json').write_text(json.dumps(assets,indent=2)+'\n')
(p/'parts.txt').write_text(''.join(a['name']+'\n' for a in assets))
print(f"Archive: {len(assets)} parts, {sum(a['size'] for a in assets)/2**30:.2f} GiB compressed")
PY
while IFS= read -r part; do
  gh release download "$tag" --repo "$repo" --pattern "$part" --dir "$download" --skip-existing
done < "$download/parts.txt"
python3 - "$download" <<'PY'
import hashlib,json,pathlib,sys
p=pathlib.Path(sys.argv[1])
for a in json.loads((p/'parts.json').read_text()):
 f=p/a['name']
 if not f.is_file() or f.stat().st_size!=a['size']: raise SystemExit(f'Incomplete part: {f}; remove the incomplete file and rerun.')
 digest=a.get('digest')
 if digest and digest.startswith('sha256:'):
  with f.open('rb') as stream: actual=hashlib.file_digest(stream,'sha256').hexdigest()
  if actual!=digest.split(':',1)[1]: raise SystemExit(f'Checksum mismatch: {f}')
 else: print(f"No published SHA-256 for {f.name}; only size and compressed-stream integrity will be checked")
PY
stage=$(mktemp -d "$root/toolchains/.css-extract.XXXXXX")
trap 'echo "Extraction did not complete; retained staging directory: $stage" >&2' ERR
echo "Extracting into $stage ..."
while IFS= read -r part; do cat "$download/$part"; done < "$download/parts.txt" | zstd -d | tar -xf - -C "$stage"
python3 - "$stage" "$root/workspace/StarterProject/FactoryGame.uproject" <<'PY'
import json,pathlib,sys
p=pathlib.Path(sys.argv[1])
project=json.loads(pathlib.Path(sys.argv[2]).read_text())
expected=project['EngineAssociation'].removesuffix('-CSS')
v=json.loads((p/'Engine/Build/Build.version').read_text())
actual='.'.join(str(v[k]) for k in ('MajorVersion','MinorVersion','PatchVersion'))
if actual!=expected: raise SystemExit(f'Engine mismatch: project requires {expected}, archive is {actual}')
for path in ('Engine/Binaries/Linux/UnrealEditor','Engine/Build/BatchFiles/Linux/Build.sh'):
 if not (p/path).is_file(): raise SystemExit(f'Missing engine file: {path}')
print(f'Engine version verified: {actual}; startup/build still need runtime verification')
PY
cp "$download/release.json" "$stage/CSS-release.json"
mv -- "$stage" "$target"
trap - ERR
printf 'Installed CSS engine: %s\nNext: enter the Distrobox, source scripts/env.sh, and run bash scripts/build.sh.\n' "$target"

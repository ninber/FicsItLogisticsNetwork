#!/usr/bin/env python3
"""Validate standalone FILN development inputs; never require another gameplay mod."""
import argparse
import json
from pathlib import Path
import sys

DEPOT_FILES = tuple(f"{kind}_{name}.uasset" for name in ("FILNDepot", "FILNFluidDepot", "FILNFluidStation", "FILNStation") for kind in ("Build", "Desc", "Recipe"))
RETIRED_ROOT = "RootGameWorldModule_FILNDepot.uasset"

def check(project, require_depot=False):
    plugin = project.parent / "Mods" / "FicsItLogisticsNetwork"
    descriptor = plugin / "FicsItLogisticsNetwork.uplugin"
    if not descriptor.is_file():
        raise ValueError(f"Required FILN source descriptor missing: {descriptor}. Run scripts/install-filn.sh.")
    metadata = json.loads(descriptor.read_text(encoding="utf-8-sig"))
    deps = metadata.get("Plugins", [])
    if (not isinstance(deps, list) or len(deps) != 1 or not isinstance(deps[0], dict)
            or deps[0].get("Name") != "SML" or deps[0].get("Enabled") is not True
            or deps[0].get("Optional", False) or not deps[0].get("SemVersion")):
        raise ValueError("Expected required versioned SML-only dependency in installed FILN descriptor; sync canonical source")
    depot = plugin / "Content" / "Depot"
    if (depot / RETIRED_ROOT).exists():
        raise ValueError("Retired generated root Blueprint present; preserve diagnostic evidence and move it outside plugin Content before building. Native root is now used.")
    if require_depot:
        for name in DEPOT_FILES:
            path = depot / name
            if not valid_package(path):
                raise ValueError(f"Required authored depot asset absent or invalid: {path}. Resolve authoring/reload before packaging.")
    return True

def valid_package(path):
    try:
        with path.open("rb") as f:
            return f.read(4) == bytes.fromhex("c1832a9e") and path.stat().st_size > 128
    except OSError:
        return False

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--require-depot", action="store_true")
    args = parser.parse_args()
    try:
        check(args.project.resolve(), args.require_depot)
    except (OSError, ValueError, TypeError) as exc:
        sys.exit("BLOCKED: " + str(exc))
    print("PASS standalone FILN descriptor and required package headers; cook and gameplay still require verification")

if __name__ == "__main__":
    main()

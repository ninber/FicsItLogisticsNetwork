#!/usr/bin/env python3
"""Fail before UBT/cook when required FILN development inputs are absent."""
import argparse
import json
from pathlib import Path
import re
import sys

DEPOT_FILES = ("Desc_FILNDepot.uasset", "Recipe_FILNDepot.uasset", "RootGameWorldModule_FILNDepot.uasset")
PIN = "f481adc3d607b0213b80e53be893b8362eed0448"

def check(project, require_depot=False):
    mods = project.parent / "Mods"
    upstream = mods / "TruckStationSettings"
    descriptor = upstream / "TruckStationSettings.uplugin"
    if not descriptor.is_file():
        raise ValueError(f"Required development dependency missing: {descriptor}. Obtain Smarter Truck Stations source separately (audited upstream {PIN}); do not copy cooked game DLLs into the Editor project.")
    metadata = json.loads(descriptor.read_text(encoding="utf-8-sig"))
    if not re.fullmatch(r"1\.1\.\d+", str(metadata.get("SemVersion", ""))):
        raise ValueError(f"Unsupported Smarter Truck Stations version: {metadata.get('SemVersion')!r}; expected 1.1.x")
    for name in ("Source/TruckStationSettings/TruckStationSettings.Build.cs", "Content/Build_ProgrammableTruckStation.uasset", "Content/Widget_FiltersTab.uasset"):
        path = upstream / name
        if not path.is_file():
            raise ValueError(f"Required dependency source/asset missing: {path}")
        if path.suffix == ".uasset" and not valid_package(path):
            raise ValueError(f"Dependency package is not an Unreal asset (possibly Git LFS pointer): {path}")
    if require_depot:
        depot = mods / "FicsItLogisticsNetwork" / "Content" / "Depot"
        for name in DEPOT_FILES:
            path = depot / name
            if not valid_package(path):
                raise ValueError(f"Required authored depot asset absent or invalid: {path}. Run headless.sh author-depot after a successful editor build; do not package source-only.")
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
    print("PASS native input existence and package headers; cook and gameplay still require verification")

if __name__ == "__main__":
    main()

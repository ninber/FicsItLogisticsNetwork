# Script-generated rest-stop placement prototype

Persistent installation: `python3 scripts/install-filn-rest-stop.py` from the repository root.
Dependency: KDataForge 2026.4.582 / SML ^3.12.0; metadata in ../kdataforge-asset.json.
Pack location: FactoryGame/DataForge/FILNRestStop; dependency: FactoryGame/Mods/KDataForge.
Both remain installed. Normal host sessions automatically unlock the custom recipe.

The current pack generates a separate descriptor, recipe and unlock. It deliberately uses
the original cooked truck-station actor and hologram, visuals, footprint and build cost.
The recipe is named FILN Rest Stop, under Vehicles. A user confirmed its menu entry.
The generated actor subclass originally attempted caused a hologram component-duplication
crash; it has been removed from the pack. No vanilla class defaults are changed.

Current limitation: the placed actor still has vanilla station inventory/fuel behavior.
The rest stop uses the cooked truck-station actor and native fuel input. Cargo-free compact visuals remain unfinished. Native refueling with the new home-first/home-last route is pending game validation.
Per-instance integration must identify custom-recipe actors using reflected mBuiltWithRecipe,
not claim all vanilla station instances as FILN bays. Scripts/RestStop.lua remains isolated.

Actual native placement-path test passed through the local player's HotKeyRecipe function;
a real hologram was created, with expected actor class and custom descriptor. Evidence:
../../../test-results/20261024T115112Z-a5b0a49f/report.json . Source save was unchanged.
Actual construction, road attachment, hold/release and saved-building reload remain unverified.

The player places the building and connects roads. With the world loaded, read-only diagnostics
are `scan` then `rest_stop_fixtures` through the existing terminal inbox helper. The latter
checks custom recipe identity, docking node GUID and arriving/leaving connection counts.
Two nonzero connection counts show attachment, not a complete directed journey proof.

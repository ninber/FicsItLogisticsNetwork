# Smarter Truck Stations integration audit (2026-11-25)

Upstream inspected: IKholopov/SM_TruckStationSettings at f481adc3d607b0213b80e53be893b8362eed0448 (plugin version 1.1.0), and SML v3.12.1. No upstream license was identified in its tree or GitHub license metadata, so no third-party implementation or Blueprint assets are copied into FILN. Use the separately installed required dependency.

## Findings

- The programmable actor, original FILTERS widget, tab injection, RCO, descriptor/recipe and schematic are authored Blueprint assets, not a C++ UI in the source file. Reuse the dependency as-is; do not create another management UI.
- The upstream hook is specific to the exact programmable truck station class, delegates LOAD to the base game, and implements UNLOAD using the FilterItem class array. Empty arrays fall back to vanilla; multiple distinct resources are excluded by FILN. Do not infer requester resource from stock.
- Upstream calls the station's AddStack followed by truck RemoveFromIndex without a destination parameter. SML FGInventoryComponent.h declares the omitted targetInventory default as nullptr. FILN's old strict transfer delegate therefore risks putting a valid programmable unload into Recovery, or losing measured delivery.
- NativeHookManager.h runs after handlers after the call scope returns, even for an overridden result. FILN observes the actual station operation's returned quantity and compares it with the exact remaining single-item truck inventory, active reserved route and station identity. The normal delegate defers programmable unload accounting; other transfers retain their existing strict endpoint checks. No duplicate cargo writes or replacement unload implementation.
- The existing hook is protected; SML access-transformer friend registration follows the published upstream integration mechanism, without copying upstream code.

## Unverified boundaries / mandatory acceptance

- Native compile/UHT/AccessTransformers and actual SML hook installation. Confirm the callback runs on the authoritative game thread. Off-thread callbacks are not allowed to mutate FILN state; if the game executes unload on a worker, this implementation will not credit delivery and must be redesigned with a thread-safe receipt bridge, not treated as passing.
- Verify exact one-filter selection and save/reload, reject blank or distinct multiple items, compare ordinary LOAD and programmable UNLOAD.
- Full and partial unload: station result, actual truck cargo delta, recipient inventory/conveyor flow and FILN counters agree exactly; no double counting, no false Recovery. Inject wrong endpoint, missing result or unexplained manual cargo change; no delivery credit.
- Verify concurrent docking, remote/significance owner change, and mod hook ordering with the installed dependency. Preserve user takeover and endpoint reservation safeguards.
- The separate authored FILN depot is still missing; upstream assets show the recipe/descriptor/buildable/schematic structure but are not licensed for copying. Native web telemetry still missing.

References:
- https://github.com/IKholopov/SM_TruckStationSettings/tree/f481adc3d607b0213b80e53be893b8362eed0448
- https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/v3.12.1/Source/FactoryGame/Public/FGInventoryComponent.h
- https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/v3.12.1/Mods/SML/Source/SML/Public/Patching/NativeHookManager.h

## Blueprint UI research

See [Smarter Truck Stations UI architecture](smarter-truck-stations-ui.md) for inspection of the real binary Blueprint packages, verified inheritance, native descriptor-list and tab references, and implications for the dedicated depot. No new UI is required in current release scope.

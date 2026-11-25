# Standalone station resource selection — current direction

User correction 2026-11-25: Satisfactory already has fluid truck stations. Remove Smarter Truck Stations completely; not even an optional adapter in the released native mod. Prior required-dependency decision is superseded. This document is an implementation handoff, not proof of completed code.

## Preserve the intended player workflow

- Build dedicated FILN depot; build ordinary LOAD and UNLOAD stations, solid or fluid as appropriate.
- Open UNLOAD station, select one compatible resource in a native-looking FILTERS tab. The selected value is the configuration; no registration or Apply/Save button beyond the normal game's selector interaction. Blank means ordinary unmanaged UNLOAD, never infer demand from contents.
- Assign vehicle using normal depot route/autopilot controls. Automatic exclusive home enrollment, overflow allocation, refuel, recovery and dispatch; web-only statistics.
- Original LOAD/UNLOAD labels and inventory/fuel tabs remain intact. No generic panel.

## Upstream evidence and independent implementation

Research upstream pinned f481adc3d607b0213b80e53be893b8362eed0448. Its Widget_ProgrammableTruckStation inherits Widget_DockingStation; its FILTERS uses BPW_Input_PartsList, BP_DescriptorListObject; its tab helper references Widget_TabsContainer and ImageAndText. Its actor references mInteractWidgetSoftClass and FilterItem; its RCO submits server changes. These serialized references are confirmed, exact graph wiring is not. See smarter-truck-stations-ui.md.

No upstream license identified. Public source availability alone does not authorize copying or redistribution; implement original FILN code/Blueprint assets against supported Satisfactory/SML interfaces. Obtain explicit license/permission before copying upstream implementation or assets. Do not depend on its plugin, classes, hooks, or content in runtime. Historical Lua experiment remains separate.

## Native API evidence (SML v3.12.1)

- FGBuildableDockingStation.h: GetDockingStationResourceForm(), GetCurrentFluidDescriptor(), GetInventory(), GetFuelInventory(), OnCurrentFluidDescriptorChanged, mIsFluidStorageInventory; Factory_LoadUnloadDockedInventory returns number transferred. Verify actual exposed/accessible methods in the matched SDK; do not guess reflection properties where public APIs suffice.
- FGWheeledVehicle.h: CanDock_Implementation(), GetDockInventoryResourceForm_Implementation(), GetStorageInventory(), GetFuelInventory(). Use supported docking-interface dispatch rather than assuming all depot/vehicle pairs compatible.
- FGInventoryComponent.h: GetAllowedItemOnIndex(), SetAllowedItemOnIndex(), GetSlotSizeForItem(), GetInventoryStacks(); saved/replicated allowed descriptors. Existing FILN ConfigureStation/ChangeStationFilters is a starting point, not proof that fluid pipe ingress respects slot filters. Verify against actual game, including current fluid descriptor changes, actual fluid unit conversion and capacity. Never use a guessed stack multiplier or infer selected resource from current content.
- Keep FILNDockReceipts.cpp's generic native result accounting if verified. No replacement inventory mutation hook unless actual vanilla native behavior proves insufficient and a safe independently tested implementation is required.

## Implementation boundaries

Use a medium-aware own configuration backend shared by solid and fluid, with native actor identity preserved. Validate authority, station/world/interaction ownership, resource form, capacity, empty, undocked, no reservations and unchanged state before mutation. Rollback on unsuccessful native readback; report rejection to player, never silently pretend success. Replicate/read back selected resource on clients, persist per station, clear only by explicit safe action; ordinary LOAD stock remains observational. No direct client mutation or global widget-tree guesses. Do not add per-tick whole-world scans.

Prefer supported SML widget Blueprint hook or original inherited station widget authoring depending on actual base-game asset graph and interaction-class API; examine both station UI assets in matched editor. Reuse the native item list and tabs, restrict to one compatible resource; use own RCO or equivalent supported authority mechanism. Do not create a new actor template solely for UI unless proven unavoidable. Test asset authoring, persistence, reload, cooking and placement independently; previous generated root module failure requires native root registration investigation and clean reload. Only enable fluid enrollment/dispatch after real compatible docking and resource enforcement exist. The same dispatcher should handle both media; depot design conditional on actual docking compatibility.

## Build and acceptance

Remove TruckStationSettings dependency from .uplugin, all native code/config/cooked references, prerequisites/packaging tests, UI assets and telemetry assumptions. Remove temporary solid-only restrictions only with complete medium validation. Preserve synchronized native transfer receipts, exclusive depot ownership, bounded scheduling, actual fuel, read-only dashboard. No fake status/placeholder acceptance. Verify absent third-party dependency in actual build and packaged imports. Build with unity and no-unity where feasible, preserve local compilation fixes and failing test evidence. Native game tests require clean isolated world, not original City; no external harness or automated UI.

The branch currently still contains mandatory dependency and solid-only guards. Local Codex changes are not present on remote. This scope update deliberately leaves C++ and asset changes to the next coordinated implementation, rather than overwriting locally compiled fixes or introducing another uncompiled UI.

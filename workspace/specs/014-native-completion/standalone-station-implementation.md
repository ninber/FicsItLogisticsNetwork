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

Native dependency removal implemented at d49e4b4: manifest/build prerequisite, reflected external class/filter discovery removed. Existing saved ExternalFilter flag is retained only as an ineligible unowned-record marker; explicit safe removal/reconfiguration required; no automatic import. Solid-only guards and own selector remain unfinished. Local Codex compilation fixes are not on remote; reconcile without losing them. Actual clean SDK build and cooked import/game acceptance remain pending.

## Fluid building source follow-up

Native root and editor authoring now cover distinct fluid depot and fluid logistics recipe; configuration, docking-medium checks, dispatch, fuel, parking, overflow and recovery accept actual compatible native forms in source. Unknown and cross-medium pairings reject. Fluid descriptor mismatch rejects; own native slot-filter readback remains mandatory. This does not establish native pipe enforcement, gas compatibility or exact transfer units. Normal independent in-game resource selector is still missing; developer console backend is not player UX. See depot-content-implementation.md, fluid-station-buildables and fluid-dispatch acceptance gates. No deployed build or actual generated assets.


## User gameplay acceptance — 2026-11-25

Development package SHA-256: `d9f1590aa7183f940ae59b67ac5e3f2535693393165431933c8f938fbcfe33b1`.
User tested only `FILN_Native_Test_20261124T222333Z.sav`; keep original City untouched.
Linux/Win64 builds, author/reload, cook, archive structural verification and installation succeeded. Actual game launch/load and performance passed in user observation. Do not interpret this as release acceptance.

- Build menu: three fluid station entries with the same vanilla name and asset. Failed distinct presentation/identification. No fluid transportation demonstrated.
- Resource selection visually works per user; source inspection finds only a marker component, not a standalone native selector wired to ConfigureStation. Backend acceptance, readback and persistence remain unverified.
- Migration appears plausible to user, but moving vehicles may simply follow their old vanilla routes. Require explicit native registration/configuration/job evidence; no migrated-station count asserted.
- Separate road network (depot, requester, provider, assigned vehicle): vehicle does nothing. Failed observed end-to-end automatic workflow; enrollment versus eligibility versus route failure not yet localized.
- Solid delivery unverified, occupied depot not tested, remaining persistence/recovery/fuel gates open.
- Dashboard not visible/no web service. Existing service watches legacy Lua heartbeat and starts Lua mode; native server requires explicit --native-dir and dashboard demand. Native telemetry is read-only and can diagnose the running session without restarting it.

Priorities: (1) obtain current native telemetry and LogFILN evidence (registry, configured requester, depot identity, vehicle enrollment, native AUTO job); (2) implement/verify actual selector-to-authority configuration and rejection/readback; (3) correct effective runtime recipe/descriptor/build-menu names, not only editor CDO text; (4) expose native dashboard lifecycle independent of Lua; (5) re-run isolated solid workflow then fluid ingress/docking/quantity/transfer and migration validation. Preserve native-only performance, safety checks, and user-controlled gameplay. Do not use developer console mutation as the player workflow or re-enable Lua/harness. Do not regenerate or deploy until fixes and required tests are ready. This report is not a fix.

## Fail-closed development policy

Portable tests and structural archive verification are component checks, never release acceptance. The separate native acceptance CI job must fail for the outstanding gates. A native recipe with a missing product, missing registration, mismatched effective recipe/descriptor name or failed build-gun availability is rejected and logged, not marked ready. This is a runtime guard, not a fix for menu naming: existing unlocked recipes are not removed from saves. Unknown road costs are never represented by fabricated zero-distance route evidence. Native dashboard sampling errors, stale snapshots and enrollment rejection are exposed. The independent player selector and native dashboard automatic lifecycle remain real implementation blockers. Do not replace them with no-op implementations or lower acceptance criteria. No runtime build/verification of these new guards has been performed in the remote editing environment.

## Native selector implementation follow-up

Inspected SML v3.12.1 binary package tables, not only name strings. Widget_DockingStation.uasset (c18eba2fa05b3e6a9b47102dae03ee431574be68) contains mInventoryContainer as VerticalBox in the generated WidgetTree; BPW_Input_PartsList.uasset (f2a2ca3726413f280b0b043c1b00a3a5aa93d2fd) contains SetMaxItemAmount(int mMaxItemAmount), SetName(FText mName), SetDescriptors(class-array mDescriptors), GetList(out class-array Items) and no-argument OnListChanged. Widget_TabsContainer.uasset (958e2e5977f3d89c185f29aa56fb86c7a2f2e148) contains mButtons, GenerateButtons() and OnButtonClicked(int ButtonIndex); ImageAndText has Text_5_FD671CE446AF05CA543E228C049AB0F9. Native controls are reused, not copied from another mod. Attach only to exact vanilla interaction class/panel and validate reflected signatures at runtime. No label rewriting, root replacement, management actions, browser controls or world scans. Only one explicit native selection, immediate reliable RPC with server station/world/proximity, medium, availability and native filter transaction readback. A clear selection removes configuration via existing safe backend. Reject stale response IDs and show authoritative selection after reply; never claim selection success from UI display alone. Storage child visibility is restored on tab change/close. Built-in parts list popup may still list incompatible choices; client/server reject incompatible selections (pre-filtering its popup is not yet demonstrated). The generated SDK's Blueprint function bodies are stubs; this serialized contract is not gameplay evidence. Actual UHT/UBT, tab layout, native list behavior, multiplayer and real solid/fluid ingress still need verification. Keep failed acceptance gates.

## Second user gameplay failure (2026-11-25)

User reports migrated depots ordinary, specialized station nonfunctional, telemetry.json missing, trucks following old repeating routes rather than dynamic assignment. Linux compile, schema-4 author/reload, Windows cook and package verification succeeded; gameplay failed. Confirmed converter wrote ordinary actor and recipe for every selected depot and external manifest is not applied automatically. Corrected solid depot class+recipe conversion with required editor receipt; external explicit resource migration still incomplete and write gate rejects normal completion. Native root now uses the SML 3.12.1 ModSubsystems construction-phase lifecycle instead of an independent late WorldSubsystem bootstrap. Startup logs and initial real snapshot independent of browser demand; telemetry timer isolated from dispatcher. Root cause of current installed game failure remains unconfirmed until actual LogFILN, SML and filesystem evidence is received. No rebuild/deploy or save changes requested at this stage; retain failures.

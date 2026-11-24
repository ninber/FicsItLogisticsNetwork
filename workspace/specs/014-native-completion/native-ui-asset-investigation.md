# Native station interaction: asset-backed root-cause investigation

2026-11-25. Baseline main 838a7f2 (post PR #3). Research only; no gameplay, save conversion, native source, deployment or runtime-harness changes.

## Observed evidence (operator handoff)

RequestInteractWidget runs on Build_TruckStation_C and Build_ProgrammableTruckStation_C. Both report root Widget_Window_DarkMode and "no supported live panel." The installed package matched its verification receipt. Therefore installation, actor resolution and RequestInteractWidget delivery are not the observed failure. Native_Init timing is not established by these lines. Native-only gameplay is smooth after removing the external UE4SS harness. FILTERS from the legacy mod is not FILN native UI. No successful native resource migration or delivery is established.

## Exact pinned assets inspected

- `Content/FactoryGame/Buildable/-Shared/Widgets/Widget_DockingStation.uasset`, Git blob `c18eba2fa05b3e6a9b47102dae03ee431574be68`, SML tag v3.12.1.
- `Content/FactoryGame/Interface/UI/Widget_Window_DarkMode.uasset`, Git blob `426a8b331887d7fd887faaa903f54ea3763362fc`, SML tag v3.12.1.
- `Content/FactoryGame/Interface/UI/InGame/SignModule/BPW_OnOffSwitch.uasset`, Git blob `b87d4fac3431750a8e7aa122bce685651a00fa46`, SML tag v3.12.1.

The read-only decoder checked UE package magic/version, name/import/export table boundaries, RootWidget, slot Content object references and the serialized setter fields. All three actual upstream binaries passed the same audit function placed in scripts/audit-native-ui-assets.cjs. No assets copied into FILN.

```json
{
  "scope": "pinned SDK asset structure only; not live widget, engine compile or gameplay acceptance",
  "station": {
    "root": "Widget_Window_DarkMode",
    "namedSlot": "WindowBody",
    "body": "CanvasPanel_68",
    "bodySlots": 25,
    "modePath": [
      "CanvasPanel_68",
      "VerticalBox_1",
      "BPW_OnOffSwitch"
    ],
    "fluidSwitch": "mForceFluidSwitch",
    "modeTextKeys": [
      "Vehicles/DockingStation/Info/Status/Load",
      "Vehicles/DockingStation/Info/Status/Unload"
    ]
  },
  "window": {
    "root": "VerticalBox_0",
    "appendContainer": "mMainContainer",
    "namedSlotPath": [
      "VerticalBox_0",
      "mMainContainer",
      "CanvasPanel_0",
      "WindowHBox",
      "WindowBody"
    ]
  },
  "switch": {
    "setter": "SetOnOffTexts",
    "serializedTextFields": [
      "mOnText",
      "mOffText"
    ],
    "authoredTextWidgets": [
      "mOnTextObject",
      "mOffTextObject"
    ]
  }
}
```

Reproduce locally without UHT, UBT or a game session:
```sh
node scripts/audit-native-ui-assets.cjs workspace/StarterProject
```
If using an external checkout, pass its exact StarterProject directory. This is intentionally a version-specific structural decoder, not a general Unreal asset parser. A hash difference is reported; a layout/format mismatch fails. It does not assert shipped cooked runtime assets or Slate visibility.

## Correct the actual attachment contract, not another broad heuristic

- The station root is a UUserWidget, not UPanelWidget. ForEachWidget explicitly excludes foreign trees. Use the root window's own live WidgetTree and its WindowBody named slot; resolve the bound body via GetContentForSlot(FName("WindowBody")) / UNamedSlot content. Verify actual ownership/instance and supported type; never mutate archetypes. The named-slot binding in the pinned station asset points to CanvasPanel_68. A nested widget may not be initialized at the initial request; if unresolved, retry on existing Native_Init or at most one next-tick interaction-scoped callback with weak refs and actual interaction identity. No continuous scans.
- The cargo canvas has 25 positioned child slots (fuel, inventory, settings etc.). Do not put a large expanding panel over it. The dark window's live mMainContainer vertical box is the verified ancestor of the WindowBody slot via CanvasPanel_0 / WindowHBox. Append a compact FILN row to this existing panel; validate actual parent chain and slot ownership. Use AddChildToVerticalBox, not InsertChildAt after Slate exists or root replacement. Keep original children/slots, tab bindings, inventory, switch and close/focus behavior. Constrain expanded content in a scroll area and allow collapse; avoid auto expansion on registered stations. An actual visual inspection remains required to ensure window size is acceptable.
- Duplicate detection and cleanup must search the same nested host (direct child of mMainContainer) or a bounded descendant traversal including nested UserWidget roots/named slots. The old FindControls and RestoreLabels traversal are outer-tree-only. On pooled reopen, reset context without adding duplicates; on RemoveInteractWidget/NativeDestruct restore only FILN-owned text, release receiver, hide/collapse row. Do not call native UI open functions automatically. Unsupported layout: one bounded dump of names/classes, named slots, parent paths, and clear failure, no arbitrary overlay.

## Correct station mode labels using the actual Blueprint API

Station canvas -> VerticalBox_1 -> BPW_OnOffSwitch is the role switch. VerticalBox_0 -> mForceFluidSwitch is a different instance of the same class; never modify it. The role switch's serialized mOnText/mOffText values reference the exact localization keys Vehicles/DockingStation/Info/Status/Load and .../Unload. The switch Blueprint exports SetOnOffTexts with two serialized FText fields mOnText/mOffText, and the authored tree contains mOnTextObject/mOffTextObject. The generated SDK may strip inner trees/function bodies. Validate actual UFunction parameter flags/type/names and instance properties before ProcessEvent; use the setter (invocation, not unsupported C++ Blueprint hooking), only after authoritative native enrollment. Preserve original FText values and restore on removal/close if still FILN-owned. Verify readback of the actual displayed text; if the setter or localization key is absent, keep vanilla labels and report a failure. Never discover labels by translated English text or patch every same-class switch. Do not change switch state or delegates. A depot is not a configured cargo station. This setter contract is a static finding, not proof of its runtime body.

## Other defects/risk points identified in main 838a7f2

- NativeOnInitialized is the sole builder of the FILN child tree. Ensure its own root, child controls and owning player exist before AddChild triggers TakeWidget. Do not reconstruct the vanilla root. The native-only console window also needs verified input/close behavior; it must not be used as an automatic workaround.
- Current SetContext automatically expands marked stations and queries server; this can make a large window, perform descriptor/vehicle enumeration, and trigger immediate RPC. Keep a compact entry and populate only on explicit panel open. One lightweight station-state query per manual interaction can set labels. Copy style from a verified existing button/label or use the Satisfactory standard-button asset if its class and delegate contract are verified, not default UMG style claims.
- ValidateContext shares a 0.2-second throttle for every RPC and silently returns on denial; a valid click right after QueryStation can disappear. Add explicit failure/readback and a scoped request sequence; avoid auto retry or timer polls. StationReply only compares Target, not interaction generation, so closing/reopening the same station can accept a stale reply. Use request IDs or one outstanding request per interaction epoch; clear on close. Guard missing RCO/subsystem/player, and report reason without disclosing distant/unauthorized station state.
- Admin checks use AFGGameSession::IsPlayerAdmin. Actual standalone/listen-host behavior is not established by SDK declarations; verify host/admin and non-admin remote separately. Do not blindly bypass server checks. A client marker is only presentation, not authority. Validate actual server registry before labeling/configuring; preserve inventory/ownership gates.
- Legacy recipe/menu object is not native FILN. Do not use FILTERS as evidence, add dependency or repurpose it. A native mod's dedicated authored rest-stop/logistics build item remains a separate asset task. Do not add legacy converter code to the DLL.

## Single local verification pass (avoid repeated package/deploy loops)

1. Run the asset audit before compilation. Add an editor-only headless FILN.NativeUI automation case exercising a nested user-widget/named-slot fixture, existing live vertical panel AddChild after Slate construction, unchanged root/children, duplicate/reopen/close cleanup; separate actual pinned Blueprint metadata test (class, named-slot binding, container, switch function FText parameters). SDK stubs may not run actual Blueprint behavior, so preserve failures and distinguish topology from runtime. No automated player UI or game launch. Run it independently of the existing failed FILN.NativeInventory stub tests; require actual named tests in the JSON report, not merely editor exit 0.
2. Only after source review and native UHT/Linux build + UI preflight pass, build/package Win64 once in the existing development container. Preserve local fixes, no nested Distrobox/Podman. Do not deploy or interrupt running game. Verify package hashes.
3. User manually opens a vanilla solid station and fluid station in a separately authorized disposable session. Verify log gives real nested panel name/class, slot attach/readback, visible compact entry, panel scroll/collapse, resource selection, feedback, ordinary labels unchanged, configured labels updated and restored, force-fluid switch untouched, repeated open/close/switch target without duplicates, no polling. Verify server mutation and reject cases before delivery tests. Log UI acceptance only after actual observation.
4. Migration stays external and incomplete. Keep original/checkpoint receipts, explicit per-station resource manifest, 74 blank templates ordinary, 3 docked excluded, 92 occupied/docked pending; register only genuinely empty/undocked/unreserved compatible stations with correct paths and verified native config/readback. Do not clear cargo or weaken ConfigureStation to force counts. If a station never becomes eligible via normal gameplay, it remains pending; no further conversion without a proven staged method and permission. No production save changes.

## References

- SML v3.12.1 asset sources and implementation: https://github.com/satisfactorymodding/SatisfactoryModLoader/tree/v3.12.1/Content/FactoryGame ; https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/v3.12.1/Mods/SML/Source/SML/Private/Kismet/SMLBlueprintWidgetLibrary.cpp ; https://github.com/satisfactorymodding/SatisfactoryModLoader/blob/v3.12.1/Mods/SML/Source/SML/Private/Patching/WidgetBlueprintHookManager.cpp
- Satisfactory window named slot: https://docs.ficsit.app/satisfactory-modding/v3.11.3/Development/BeginnersGuide/SimpleMod/machines/SimpleInteraction.html
- Unreal traversal distinction: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/Blueprint/UWidgetTree/ForEachWidget?application_version=5.5 ; https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Blueprint/UWidgetTree/ForEachWidgetAndDescendants?application_version=5.5
- Live insertion caveat: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/UPanelWidget/InsertChildAt
- Pool lifetime: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/UMG/Blueprint/FUserWidgetPool?application_version=5.5
- SML widget and RCO hooks: https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/WidgetBlueprintHooks.html ; https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html

The research does not claim a new DLL, successful UI, migrated city, or new runtime acceptance.

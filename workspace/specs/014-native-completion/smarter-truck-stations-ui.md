# Smarter Truck Stations: native-looking UI architecture

Research date: 2026-11-25. Upstream pinned commit f481adc3d607b0213b80e53be893b8362eed0448, v1.1.0.

## Evidence and limits

Downloaded the actual binary UE package assets as base64 through the repository file API, decoded them, confirmed package magic C1 83 2A 9E and inspected serialized printable names, imported class paths, generated-class and parent-class asset registry tags. This is stronger than inferring architecture from filenames, but is not a parsed Blueprint graph, graph wiring, property flags, runtime or visual test. No upstream asset was copied into FILN. No explicit license was found.

## Confirmed parent classes and asset relationships

| Asset | Parent / relevant references |
| --- | --- |
| Build_ProgrammableTruckStation | Blueprint child of /Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C; native parent FGBuildableDockingStation. Contains mInteractWidgetSoftClass, Widget_ProgrammableTruckStation_C, FilterItem, GetFilterItems, SetFilterItem, UpdateFilterItem, OnRep_FilterItem, FlushNetDormancy. |
| Widget_ProgrammableTruckStation | Widget Blueprint child of /Game/FactoryGame/Buildable/-Shared/Widgets/Widget_DockingStation.Widget_DockingStation_C; native parent FGInteractWidget. References vanilla Widget_UseableBase and Widget_Window_DarkMode, mInteractObject, TabBox, TabWidgetSwitcher, Widget_FiltersTab, Widget_TabBox_Inject, GetChildAt, RemoveFromParent, AddChild, OnSaveListHandler, GetCurrentFilter, GetFilterItems, SetServerFilterValue, UpdateUnloadFilter, SetUpFiltersContent, SetStartupList, OnSyncUnloadList, GetRemoteCallObjectOfClass. |
| Widget_FiltersTab | UMG UserWidget. References /Game/FactoryGame/Interface/UI/InGame/-Shared/InputPartsList/BPW_Input_PartsList and /Game/FactoryGame/Interface/UI/InGame/Splitter/BP_DescriptorListObject; names GetAllPickupableItemDescriptors, SetDescriptors, SetDescriptorListObjects, SetStartupList, GetList, OnSyncUnloadList, SetUpFiltersContent, FilterScrollBox, Unload filters. |
| Widget_TabBox_Inject | UMG UserWidget. References /Game/FactoryGame/Interface/UI/InGame/-Shared/Widget_TabsContainer, /Game/FactoryGame/Interface/UI/InGame/-Shared/ImageAndText and default_tab_icon. Names GenerateButtons, mButtons, mButtonSize, mOverrideButtonFunctions, OnButtonClicked, TabWidgetSwitcher, SetActiveWidgetIndex and Filters. |
| RCO_ProgrammableTruckStation | Child of /Game/FactoryGame/Character/Player/BP_RemoteCallObject.BP_RemoteCallObject_C; native FGRemoteCallObject. Names SetServerFilterValue, building, Items, UpdateFilterItem. |
| RootInstance_TruckStationSettings | SML GameInstanceModule with RemoteCallObjects and programmable RCO reference. |
| RootGameWorldModule_TruckStationSettings | SML GameWorldModule with mSchematics and Schematic_SmarterTrucks reference. |
| Desc / Recipe / Schematic | Descriptor points to custom buildable and vanilla Vehicles/Transport category/icon; recipe points to descriptor; schematic references recipe and BP_UnlockRecipe. |

## Likely flow, to verify in editor before making changes

Construction recipe selects programmable truck station actor; its interaction class is the child of the vanilla station widget. That widget integrates a FILTERS content tab using a tab container and game item-list components; it reads the interaction object and existing selected filter, and appears to submit edits through the remote call object to the actor. Actor replication notification names indicate replicated filter synchronization, but exact replication flags, SaveGame flags, authority and validation are not established by string inspection. Do not claim these verified without graph/property inspection.

This is not the same architecture as global AFGHUD::RequestInteractWidget/UFGInteractWidget::Native_Init hooks attaching arbitrary UVerticalBox or Canvas content. The previous FILN UI had no vanilla widget inheritance or native descriptor list and relied on heuristic live panel search, raw controls, and label replacement. These were removed from the player workflow.

## Decision for FILN

- Current release: use installed Smarter Truck Stations unchanged. Its UI owns resource selection; FILN reads the exact one-solid-item filter, observes actual cargo operations, and does not inject its own panel.
- Depot: author a separate buildable/recipe/descriptor/schematic chain from vanilla TruckStation (or use a supported recipe-identified vanilla actor approach only if proven) and register it through SML. Give it ordinary interaction UI, fuel and automatic parking; no manual registration/management menu. Verify cooking and real placement, docking, path node identity, fuel and save reload. Existing Lua recipe-identified depot is the behavioral baseline; the implementation choice must account for uninstall and existing road/actor identity.
- Future optional UI: author a Widget Blueprint child of the appropriate original station widget, use base-game item-list and tab components, reference it on the dedicated buildable's interaction class, expose only required C++ data via Blueprint-callable methods. If a global widget modification is ever genuinely needed, investigate SML Widget Blueprint Hooks rather than guessing arbitrary live panels. Do not change LOAD/UNLOAD labels unless separately requested and verified.
- Blueprint content is compatible with a native C++ dispatcher; native does not mean every presentation element must be created in C++. No Lua or UE4SS runtime required.
- No standalone selector or fluid UI in current scope. Statistics remain in web dashboard. Do not restart UI implementation or install third-party assets without permission.

## References

- https://github.com/IKholopov/SM_TruckStationSettings/tree/f481adc3d607b0213b80e53be893b8362eed0448/Content
- https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/index.html
- https://docs.ficsit.app/satisfactory-modding/v3.11.3/Development/BeginnersGuide/SimpleMod/machines/SimpleInteraction.html
- https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/WidgetBlueprintHooks.html
- https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/ModModules.html

Research only: no UI assets generated, compiled or loaded in game.

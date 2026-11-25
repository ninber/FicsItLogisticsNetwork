"""Run ONLY in the matching Unreal Editor Python commandlet, never in the game.

Create original FILN-owned solid/fluid descriptor/recipe Blueprints using game
parents. The SML root module is native C++. No runtime-generated actor class, no upstream mod asset copying.
Existing assets are validated and never overwritten.
"""
import json
import os
from pathlib import Path
import unreal

DEST = "/FicsItLogisticsNetwork/Depot"
VALIDATE_ONLY = os.environ.get("FILN_DEPOT_VALIDATE_ONLY") == "1"

def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value

def prop(obj, *names):
    for name in names:
        try:
            return obj.get_editor_property(name), name
        except Exception:
            pass
    raise RuntimeError(f"Missing expected property {names} on {obj}")

def assign(obj, value, *names):
    _, name = prop(obj, *names)
    obj.set_editor_property(name, value)

def cls(path):
    return require(unreal.load_class(None, path), f"Missing class {path}")

def package(name, parent, configure, validate):
    path = DEST + "/" + name
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(path):
        bp = require(assets.load_asset(path), f"Cannot load {path}")
        existing = True
    else:
        require(not VALIDATE_ONLY, f"Fresh reload cannot find {path}")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Blueprint, factory), f"Cannot create {path}")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        existing = False
    generated = require(assets.load_blueprint_class(path), f"No generated class {path}")
    require(prop(bp, "parent_class")[0] == parent, f"Unexpected parent: {path}")
    cdo = unreal.get_default_object(generated)
    if not existing:
        configure(cdo)
    validate(cdo)
    if not existing:
        require(assets.save_loaded_asset(bp, only_if_is_dirty=False), f"Save failed: {path}")
    return generated

def vanilla_recipe(short_name, expected_descriptor, fluid):
    # Registry lookup by exact known recipe asset name, not guessed package paths.
    # Reject duplicates and validate descriptor, actor, cargo medium and producer.
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    candidates = [a for a in registry.get_assets_by_path("/Game/FactoryGame", recursive=True)
                  if str(a.asset_name) == short_name]
    require(len(candidates) == 1, f"Expected exactly one vanilla recipe {short_name}; found {len(candidates)}")
    recipe = require(unreal.EditorAssetLibrary.load_blueprint_class(str(candidates[0].package_name)), f"Cannot load {short_name}")
    cdo = unreal.get_default_object(recipe)
    products = prop(cdo, "m_product", "mProduct")[0]
    require(len(products) == 1 and prop(products[0], "amount", "Amount")[0] == 1, f"Invalid vanilla recipe {short_name}")
    desc = prop(products[0], "item_class", "ItemClass")[0]
    require(desc and desc.get_name() == expected_descriptor, f"Wrong product of {short_name}")
    actor = prop(unreal.get_default_object(desc), "m_buildable_class", "mBuildableClass")[0]
    require(actor, f"No actor for {expected_descriptor}")
    actor_cdo = unreal.get_default_object(actor)
    require(bool(prop(actor_cdo, "m_is_fluid_storage_inventory", "is_fluid_storage_inventory", "mIsFluidStorageInventory")[0]) == fluid, f"Incorrect cargo medium: {short_name}")
    require(prop(cdo, "m_ingredients", "mIngredients")[0] and prop(cdo, "m_produced_in", "mProducedIn")[0], f"Missing vanilla recipe cost/producer: {short_name}")
    return recipe, desc, actor

def author(suffix, label, parents):
    recipe_parent, desc_parent, actor = parents
    def desc_config(cdo):
        assign(cdo, actor, "m_buildable_class", "mBuildableClass")
        assign(cdo, unreal.Text(label), "m_display_name", "mDisplayName")
    def desc_validate(cdo):
        require(prop(cdo, "m_buildable_class", "mBuildableClass")[0] == actor, "Depot must reuse the cooked vanilla actor")
        require(str(prop(cdo, "m_display_name", "mDisplayName")[0]) == label, "Depot descriptor name mismatch")
    desc = package("Desc_" + suffix, desc_parent, desc_config, desc_validate)
    def recipe_config(cdo):
        amount = unreal.ItemAmount()
        assign(amount, desc, "item_class", "ItemClass")
        assign(amount, 1, "amount", "Amount")
        assign(cdo, [amount], "m_product", "mProduct")
        assign(cdo, True, "m_display_name_override", "mDisplayNameOverride")
        assign(cdo, unreal.Text(label), "m_display_name", "mDisplayName")
    def recipe_validate(cdo):
        products = prop(cdo, "m_product", "mProduct")[0]
        require(len(products) == 1 and prop(products[0], "item_class", "ItemClass")[0] == desc and prop(products[0], "amount", "Amount")[0] == 1, "Incorrect depot recipe product")
        require(prop(cdo, "m_display_name_override", "mDisplayNameOverride")[0], "Depot recipe name override missing")
        require(str(prop(cdo, "m_display_name", "mDisplayName")[0]) == label, "Depot recipe name mismatch")
        require(prop(cdo, "m_ingredients", "mIngredients")[0], "Inherited build cost missing")
        require(prop(cdo, "m_produced_in", "mProducedIn")[0], "Inherited build gun producer missing")
    recipe = package("Recipe_" + suffix, recipe_parent, recipe_config, recipe_validate)
    return {"descriptor": desc.get_path_name(), "recipe": recipe.get_path_name(), "actor": actor.get_path_name(), "vanilla_recipe": recipe_parent.get_path_name()}

def main():
    assets = unreal.EditorAssetLibrary
    if VALIDATE_ONLY:
        require(assets.does_directory_exist(DEST), "Fresh reload missing content directory")
    else:
        require(assets.make_directory(DEST) or assets.does_directory_exist(DEST), "Cannot create depot content directory")
    solid = vanilla_recipe("Recipe_TruckStation", "Desc_TruckStation_C", False)
    fluid = vanilla_recipe("Recipe_FluidTruckStation", "Desc_FluidTruckStation_C", True)
    result = {"schema": 2, "buildings": [
        author("FILNDepot", "FILN Depot", solid),
        author("FILNFluidDepot", "FILN Fluid Depot", fluid),
        author("FILNFluidStation", "FILN Fluid Logistics Station", fluid)
    ]}
    receipt = Path(os.environ["FILN_DEPOT_AUTHOR_RECEIPT"])
    receipt.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("FILN_STATION_ASSETS_VALIDATED " + json.dumps(result))

main()

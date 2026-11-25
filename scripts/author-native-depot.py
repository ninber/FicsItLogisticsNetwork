"""Run ONLY in the matching Unreal Editor Python commandlet, never in the game.

Create original FILN-owned descriptor/recipe/root module Blueprints using game
parents. No runtime-generated actor class, no upstream mod asset copying.
Existing assets are validated and never overwritten.
"""
import json
import os
from pathlib import Path
import unreal

DEST = "/FicsItLogisticsNetwork/Depot"
VANILLA_DESC = "/Game/FactoryGame/Buildable/Factory/TruckStation/Desc_TruckStation.Desc_TruckStation_C"
VANILLA_RECIPE = "/Game/FactoryGame/Recipes/Buildings/Recipe_TruckStation.Recipe_TruckStation_C"
VANILLA_ACTOR = "/Game/FactoryGame/Buildable/Factory/TruckStation/Build_TruckStation.Build_TruckStation_C"
MODULE = "/Script/FicsItLogisticsNetwork.FILNDepotModule"

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
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Blueprint, factory), f"Cannot create {path}")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        existing = False
    generated = require(assets.load_blueprint_class(path), f"No generated class {path}")
    cdo = unreal.get_default_object(generated)
    if not existing:
        configure(cdo)
    validate(cdo)
    if not existing:
        require(assets.save_loaded_asset(bp, only_if_is_dirty=False), f"Save failed: {path}")
    return generated

def main():
    assets = unreal.EditorAssetLibrary
    require(assets.make_directory(DEST) or assets.does_directory_exist(DEST), "Cannot create depot content directory")
    actor = cls(VANILLA_ACTOR)
    def desc_config(cdo):
        assign(cdo, actor, "m_buildable_class", "mBuildableClass")
        assign(cdo, unreal.Text("FILN Depot"), "m_display_name", "mDisplayName")
    def desc_validate(cdo):
        require(prop(cdo, "m_buildable_class", "mBuildableClass")[0] == actor, "Depot must reuse the cooked vanilla actor")
        require(str(prop(cdo, "m_display_name", "mDisplayName")[0]) == "FILN Depot", "Depot descriptor name mismatch")
    desc = package("Desc_FILNDepot", cls(VANILLA_DESC), desc_config, desc_validate)
    def recipe_config(cdo):
        amount = unreal.ItemAmount()
        assign(amount, desc, "item_class", "ItemClass")
        assign(amount, 1, "amount", "Amount")
        assign(cdo, [amount], "m_product", "mProduct")
        assign(cdo, True, "m_display_name_override", "mDisplayNameOverride")
        assign(cdo, unreal.Text("FILN Depot"), "m_display_name", "mDisplayName")
    def recipe_validate(cdo):
        products = prop(cdo, "m_product", "mProduct")[0]
        require(len(products) == 1 and prop(products[0], "item_class", "ItemClass")[0] == desc and prop(products[0], "amount", "Amount")[0] == 1, "Incorrect depot recipe product")
        require(prop(cdo, "m_display_name_override", "mDisplayNameOverride")[0], "Depot recipe name override missing")
        require(str(prop(cdo, "m_display_name", "mDisplayName")[0]) == "FILN Depot", "Depot recipe name mismatch")
        require(prop(cdo, "m_ingredients", "mIngredients")[0], "Inherited build cost missing")
        require(prop(cdo, "m_produced_in", "mProducedIn")[0], "Inherited build gun producer missing")
    recipe = package("Recipe_FILNDepot", cls(VANILLA_RECIPE), recipe_config, recipe_validate)
    def root_config(cdo):
        assign(cdo, True, "root_module", "b_root_module", "bRootModule")
        assign(cdo, recipe, "depot_recipe", "DepotRecipe")
    def root_validate(cdo):
        require(prop(cdo, "root_module", "b_root_module", "bRootModule")[0], "Root module flag missing")
        require(prop(cdo, "depot_recipe", "DepotRecipe")[0] == recipe, "Root recipe reference mismatch")
    module = package("RootGameWorldModule_FILNDepot", cls(MODULE), root_config, root_validate)
    result = {"schema": 1, "descriptor": desc.get_path_name(), "recipe": recipe.get_path_name(), "module": module.get_path_name(), "actor": actor.get_path_name()}
    receipt = Path(os.environ["FILN_DEPOT_AUTHOR_RECEIPT"])
    receipt.write_text(json.dumps(result, indent=2), encoding="utf-8")
    unreal.log("FILN_DEPOT_ASSETS_AUTHORED " + json.dumps(result))

main()

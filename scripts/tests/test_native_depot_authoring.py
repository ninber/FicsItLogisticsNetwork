"""Portable regression of authoring callbacks; does not run Unreal or cook assets."""
import ast
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import sys


class EditorObject:
    def __init__(self, path="", **props):
        self.path = path
        self.props = props

    def get_path_name(self):
        return self.path

    def get_editor_property(self, name):
        if name not in self.props:
            raise AttributeError("Property is not editor-accessible: " + name)
        return self.props[name]

    def set_editor_property(self, name, value):
        self.get_editor_property(name)
        self.props[name] = value


class NativeDepotAuthoringTests(unittest.TestCase):
    def setUp(self):
        script = Path(__file__).resolve().parents[1] / "author-native-depot.py"
        tree = ast.parse(script.read_text(encoding="utf-8"), filename=str(script))
        # Load the actual commandlet callbacks without launching its entry point.
        self.assertEqual(ast.unparse(tree.body[-1]), "main()")
        tree.body.pop()
        fake_unreal = SimpleNamespace(
            Text=str,
            ItemAmount=lambda: EditorObject(item_class=None, amount=0),
            get_default_object=lambda obj: obj,
        )
        self.namespace = {}
        with patch.dict(sys.modules, {"unreal": fake_unreal}):
            exec(compile(tree, str(script), "exec"), self.namespace)
        self.actor = EditorObject("/Game/Actor", m_is_fluid_storage_inventory=False, m_display_name="Vanilla")
        self.recipe_parent = EditorObject("/Game/Recipe")
        self.desc_parent = EditorObject("/Game/Descriptor")
        self.created = {}

    def run_author(self, suffix, label, corrupt=None, upgrade=False, unexpected=False):
        self.namespace["UPGRADE_NAMES"] = upgrade
        def package(name, parent, configure, validate, upgrade=None):
            if name.startswith("Build_"):
                self.assertIs(parent, self.actor)
                cdo = EditorObject(name, m_display_name="Vanilla", m_is_fluid_storage_inventory=self.actor.props["m_is_fluid_storage_inventory"])
            elif name.startswith("Desc_"):
                self.assertIs(parent, self.desc_parent)
                cdo = EditorObject(name, m_buildable_class=self.actor, m_display_name="Vanilla")
            else:
                self.assertIs(parent, self.recipe_parent)
                # Deliberately no accessible override property, as in the SDK.
                cdo = EditorObject(name, m_product=[], m_display_name="Vanilla",
                                   m_ingredients=["Cost"], m_produced_in=["BuildGun"])
            if name.startswith("Desc_") and self.namespace["UPGRADE_NAMES"]:
                cdo.props["m_display_name"] = label
                if unexpected:
                    cdo.props["m_buildable_class"] = EditorObject("/Game/Unexpected")
                self.assertTrue(upgrade(cdo))
            else:
                configure(cdo)
            if corrupt:
                corrupt(name, cdo)
            validate(cdo)
            self.created[name] = cdo
            return cdo

        self.namespace["package"] = package
        return self.namespace["author"](suffix, label, (self.recipe_parent, self.desc_parent, self.actor))

    def test_all_four_buildings_without_override_property(self):
        for suffix, label in (("FILNDepot", "FILN Depot"),
                              ("FILNFluidDepot", "FILN Fluid Depot"),
                              ("FILNFluidStation", "FILN Fluid Logistics Station"),
                              ("FILNStation", "FILN Logistics Station")):
            with self.subTest(suffix=suffix):
                receipt = self.run_author(suffix, label)
                desc = self.created["Desc_" + suffix]
                recipe = self.created["Recipe_" + suffix]
                self.assertEqual(self.created["Build_" + suffix].props["m_display_name"], label)
                self.assertIs(desc.props["m_buildable_class"], self.created["Build_" + suffix])
                self.assertEqual(self.actor.props["m_display_name"], "Vanilla")
                self.assertEqual(desc.props["m_display_name"], label)
                self.assertEqual(recipe.props["m_display_name"], label)
                self.assertIs(recipe.props["m_product"][0].props["item_class"], desc)
                self.assertEqual(recipe.props["m_product"][0].props["amount"], 1)
                self.assertEqual(receipt["display_name"], label)

    def test_main_authors_all_four_buildable_roles(self):
        calls = []
        self.namespace["unreal"].EditorAssetLibrary = SimpleNamespace(
            make_directory=lambda _: True, does_directory_exist=lambda _: True)
        self.namespace["unreal"].log = lambda _: None
        self.namespace["vanilla_recipe"] = lambda *args: args
        self.namespace["author"] = lambda suffix, label, parents: calls.append((suffix, parents[2])) or {"name": suffix}
        self.namespace["VALIDATE_ONLY"] = False
        with patch.dict("os.environ", {"FILN_DEPOT_AUTHOR_RECEIPT": "unused.json"}):
            with patch.object(Path, "write_text"):
                self.namespace["main"]()
        self.assertEqual(set(calls), {
            ("FILNDepot", False), ("FILNFluidDepot", True),
            ("FILNFluidStation", True), ("FILNStation", False)})
        self.assertEqual(len(calls), 4)

    def test_known_descriptor_upgrade_repoints_only_filn_descriptor(self):
        self.run_author("FILNFluidDepot", "FILN Fluid Depot", upgrade=True)
        self.assertIs(self.created["Desc_FILNFluidDepot"].props["m_buildable_class"], self.created["Build_FILNFluidDepot"])
        self.assertEqual(self.actor.props["m_display_name"], "Vanilla")
        with self.assertRaisesRegex(RuntimeError, "Refusing to overwrite"):
            self.run_author("FILNFluidDepot", "FILN Fluid Depot", upgrade=True, unexpected=True)

    def test_wrong_actor_name_or_medium_fails(self):
        for key, value, error in (("m_display_name", "Vanilla", "buildable name mismatch"),
                                  ("m_is_fluid_storage_inventory", True, "cargo medium mismatch")):
            def corrupt(name, cdo):
                if name.startswith("Build_"):
                    cdo.props[key] = value
            with self.subTest(key=key), self.assertRaisesRegex(RuntimeError, error):
                self.run_author("FILNDepot", "FILN Depot", corrupt)

    def test_wrong_recipe_name_still_fails(self):
        def corrupt(name, cdo):
            if name.startswith("Recipe_"):
                cdo.props["m_display_name"] = "Vanilla"
        with self.assertRaisesRegex(RuntimeError, "recipe name mismatch"):
            self.run_author("FILNDepot", "FILN Depot", corrupt)

    def test_wrong_descriptor_name_still_fails(self):
        def corrupt(name, cdo):
            if name.startswith("Desc_"):
                cdo.props["m_display_name"] = "Vanilla"
        with self.assertRaisesRegex(RuntimeError, "descriptor name mismatch"):
            self.run_author("FILNDepot", "FILN Depot", corrupt)

    def test_wrong_product_still_fails(self):
        def corrupt(name, cdo):
            if name.startswith("Recipe_"):
                cdo.props["m_product"][0].props["amount"] = 2
        with self.assertRaisesRegex(RuntimeError, "Incorrect depot recipe product"):
            self.run_author("FILNDepot", "FILN Depot", corrupt)

    def test_missing_cost_or_producer_still_fails(self):
        for field, error in (("m_ingredients", "build cost missing"),
                             ("m_produced_in", "build gun producer missing")):
            def corrupt(name, cdo):
                if name.startswith("Recipe_"):
                    cdo.props[field] = []
            with self.subTest(field=field):
                with self.assertRaisesRegex(RuntimeError, error):
                    self.run_author("FILNDepot", "FILN Depot", corrupt)


if __name__ == "__main__":
    unittest.main()

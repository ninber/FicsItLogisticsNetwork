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
        )
        self.namespace = {}
        with patch.dict(sys.modules, {"unreal": fake_unreal}):
            exec(compile(tree, str(script), "exec"), self.namespace)
        self.actor = EditorObject("/Game/Actor")
        self.recipe_parent = EditorObject("/Game/Recipe")
        self.desc_parent = EditorObject("/Game/Descriptor")
        self.created = {}

    def run_author(self, suffix, label, corrupt=None):
        def package(name, parent, configure, validate):
            if name.startswith("Desc_"):
                self.assertIs(parent, self.desc_parent)
                cdo = EditorObject(name, m_buildable_class=self.actor, m_display_name="Vanilla")
            else:
                self.assertIs(parent, self.recipe_parent)
                # Deliberately no accessible override property, as in the SDK.
                cdo = EditorObject(name, m_product=[], m_display_name="Vanilla",
                                   m_ingredients=["Cost"], m_produced_in=["BuildGun"])
            configure(cdo)
            if corrupt:
                corrupt(name, cdo)
            validate(cdo)
            self.created[name] = cdo
            return cdo

        self.namespace["package"] = package
        return self.namespace["author"](suffix, label, (self.recipe_parent, self.desc_parent, self.actor))

    def test_all_three_buildings_without_override_property(self):
        for suffix, label in (("FILNDepot", "FILN Depot"),
                              ("FILNFluidDepot", "FILN Fluid Depot"),
                              ("FILNFluidStation", "FILN Fluid Logistics Station")):
            with self.subTest(suffix=suffix):
                receipt = self.run_author(suffix, label)
                desc = self.created["Desc_" + suffix]
                recipe = self.created["Recipe_" + suffix]
                self.assertEqual(desc.props["m_display_name"], label)
                self.assertEqual(recipe.props["m_display_name"], label)
                self.assertIs(recipe.props["m_product"][0].props["item_class"], desc)
                self.assertEqual(recipe.props["m_product"][0].props["amount"], 1)
                self.assertEqual(receipt["display_name"], label)

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

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SCRIPTS=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("native_prereqs",SCRIPTS/"check-native-prerequisites.py")
mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)

class NativePrerequisiteTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        self.project=self.root/"FactoryGame.uproject"
        self.project.write_text("{}")
        self.plugin=self.root/"Mods/FicsItLogisticsNetwork"
        self.depot=self.plugin/"Content/Depot"
    def put(self,path,data):
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)
    def install_descriptor(self, extra=()):
        deps=[{"Name":"SML","Enabled":True,"SemVersion":">=3.12.1 <3.13.0"},*extra]
        self.put(self.plugin/"FicsItLogisticsNetwork.uplugin",json.dumps({"Plugins":deps}).encode())
    def test_missing_filn_descriptor_blocks(self):
        with self.assertRaisesRegex(ValueError,"FILN source descriptor missing"):
            mod.check(self.project)
    def test_standalone_without_other_mod_installed(self):
        self.install_descriptor()
        self.assertTrue(mod.check(self.project))
        self.assertFalse((self.root/"Mods/TruckStationSettings").exists())
    def test_old_dependency_rejected_even_optional_or_disabled(self):
        for enabled,optional in ((True,False),(True,True),(False,False)):
            with self.subTest(enabled=enabled,optional=optional):
                self.install_descriptor([{"Name":"TruckStationSettings","Enabled":enabled,"Optional":optional,"SemVersion":"1.1.0"}])
                with self.assertRaisesRegex(ValueError,"SML-only"):
                    mod.check(self.project)
    def test_build_allows_authoring_but_package_requires_depot(self):
        self.install_descriptor()
        self.assertTrue(mod.check(self.project))
        with self.assertRaisesRegex(ValueError,"authored depot asset"):
            mod.check(self.project,True)
        self.assertEqual(len(mod.DEPOT_FILES), 12)
        self.assertIn("Build_FILNStation.uasset", mod.DEPOT_FILES)
        for name in mod.DEPOT_FILES:
            self.put(self.depot/name,bytes.fromhex("c1832a9e")+b"x"*150)
        self.assertTrue(mod.check(self.project,True))
        self.put(self.depot/mod.DEPOT_FILES[0],b"version https://git-lfs.github.com/spec/v1")
        with self.assertRaisesRegex(ValueError,"authored depot asset"):
            mod.check(self.project,True)
    def test_retired_root_blocks_even_before_authoring(self):
        self.install_descriptor()
        self.put(self.depot/mod.RETIRED_ROOT,b"old asset")
        with self.assertRaisesRegex(ValueError,"Retired generated root Blueprint"):
            mod.check(self.project)
    def test_canonical_native_source_has_no_external_runtime_imports(self):
        root=SCRIPTS.parent/"workspace/mods/FicsItLogisticsNetwork"
        descriptor=json.loads((root/"FicsItLogisticsNetwork.uplugin").read_text())
        self.assertEqual([p["Name"] for p in descriptor["Plugins"]],["SML"])
        for folder in ("Source","Config"):
            for path in (root/folder).rglob("*"):
                if path.is_file() and path.suffix in (".h",".cpp",".cs",".ini"):
                    content=path.read_text(encoding="utf-8-sig")
                    for token in ("TruckStationSettings","Build_ProgrammableTruckStation","KDataForge","UE4SS"):
                        self.assertNotIn(token,content,str(path))

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
        self.up=self.root/"Mods/TruckStationSettings"
        self.depot=self.root/"Mods/FicsItLogisticsNetwork/Content/Depot"
    def put(self,path,data):
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(data)
    def install_dependency(self):
        self.put(self.up/"TruckStationSettings.uplugin",json.dumps({"SemVersion":"1.1.0"}).encode())
        self.put(self.up/"Source/TruckStationSettings/TruckStationSettings.Build.cs",b"// test fixture")
        for name in ("Build_ProgrammableTruckStation.uasset","Widget_FiltersTab.uasset"):
            self.put(self.up/"Content"/name,bytes.fromhex("c1832a9e")+b"x"*150)
    def test_absent_dependency_blocks_before_compiler(self):
        with self.assertRaisesRegex(ValueError,"Required development dependency missing"):
            mod.check(self.project)
    def test_dependency_header_only_does_not_prove_assets(self):
        self.install_dependency()
        self.put(self.up/"Content/Widget_FiltersTab.uasset",b"version https://git-lfs.github.com/spec/v1")
        with self.assertRaisesRegex(ValueError,"not an Unreal asset"):
            mod.check(self.project)
    def test_build_allows_authoring_but_package_requires_depot(self):
        self.install_dependency()
        self.assertTrue(mod.check(self.project))
        with self.assertRaisesRegex(ValueError,"authored depot asset"):
            mod.check(self.project,True)
        for name in mod.DEPOT_FILES:
            self.put(self.depot/name,bytes.fromhex("c1832a9e")+b"x"*150)
        self.assertTrue(mod.check(self.project,True))
    def test_incompatible_dependency_version(self):
        self.install_dependency()
        self.put(self.up/"TruckStationSettings.uplugin",b'{"SemVersion":"2.0.0"}')
        with self.assertRaisesRegex(ValueError,"Unsupported"):
            mod.check(self.project)

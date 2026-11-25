from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from filn_dashboard_lifecycle import ModLifetime, fresh_heartbeat, game_processes, NativeModLifetime, native_world, native_service_units

class LifetimeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.path=Path(self.temp.name)/'heartbeat';self.now=100;self.processes={}
    def tearDown(self):self.temp.cleanup()
    def lifetime(self):return ModLifetime(self.path,lambda:self.now,lambda:self.processes)
    def test_login_without_running_mod_does_not_start_server(self):
        self.assertFalse(self.lifetime().alive())
        self.path.write_text('10');self.assertFalse(self.lifetime().alive())
    def test_expired_mod_heartbeat_does_not_fall_back_to_a_running_game(self):
        self.path.write_text('10');self.processes={'42':'123'}
        self.assertFalse(self.lifetime().alive())
    def test_mod_heartbeat_starts_and_expiry_stops(self):
        self.path.write_text('100');life=self.lifetime();self.assertTrue(life.alive())
        self.now=129;self.assertTrue(life.alive());self.now=130;self.assertFalse(life.alive())
    def test_mod_refresh_extends_lifetime_and_invalid_file_stops(self):
        self.path.write_text('100');life=self.lifetime();self.now=125;self.path.write_text('125');self.assertTrue(life.alive())
        self.path.write_text('');self.assertFalse(life.alive())
    def test_existing_game_migration_stops_on_exit_and_does_not_reattach(self):
        self.processes={'42':'123'};life=self.lifetime();self.assertTrue(life.alive())
        self.processes={'43':'125'};self.assertFalse(life.alive())
        self.processes={'42':'126'};self.assertFalse(life.alive())
    def test_heartbeat_takes_over_existing_game_then_stops_with_mod(self):
        self.processes={'42':'123'};life=self.lifetime();self.path.write_text('100');self.assertTrue(life.alive())
        self.now=131;self.assertFalse(life.alive())
    def test_future_or_malformed_stamp_not_alive(self):
        for stamp in ['oops','9999','NaN','inf']:
            self.path.write_text(stamp);self.assertFalse(fresh_heartbeat(self.path,100))
    def test_process_selection_excludes_steam_and_other_games(self):
        root=Path(self.temp.name)
        for pid,comm,command in [('42','FactoryGameStea','C:\\game\\FactoryGameSteam-Win64-Shipping.exe'),('43','steam','steam'),('44','FactoryGameOther','other.exe')]:
            p=root/pid;p.mkdir();(p/'comm').write_text(comm);(p/'cmdline').write_bytes(command.encode()+b'\0');(p/'stat').write_text(pid+' (game) '+' '.join(['S']+['0']*18+['123']))
        self.assertEqual(game_processes(root),{'42':'123'})

class NativeLifetimeTests(unittest.TestCase):
    def test_native_never_uses_lua_or_process_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'dashboard-native-mod.txt'
            self.assertFalse(NativeModLifetime(path,lambda:100).alive())
            path.write_text('100')
            self.assertFalse(NativeModLifetime(path,lambda:100).alive())
            path.write_text('100\nabc123\n')
            life=NativeModLifetime(path,lambda:100)
            self.assertTrue(life.alive())
            path.write_text('100\ndef456\n')
            self.assertFalse(life.alive())
    def test_native_expiry_and_removal(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'dashboard-native-mod.txt'
            path.write_text('100\nabc123\n')
            self.assertEqual(native_world(path,129), 'abc123')
            self.assertIsNone(native_world(path,130))
            path.unlink()
            self.assertIsNone(native_world(path,100))
    def test_native_service_is_not_enabled_at_login(self):
        unit, watch=native_service_units(Path('/home/test/root'),Path('/home/test/Saved/FILN'))
        self.assertIn('--native-dir "/home/test/Saved/FILN" --with-game-mod',unit)
        self.assertNotIn('[Install]',unit)
        self.assertIn('PathChanged=/home/test/Saved/FILN/dashboard-native-mod.txt',watch)
        self.assertNotIn('FILNLua',unit+watch)

if __name__=='__main__':unittest.main()

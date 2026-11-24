import json,sys,tempfile,time,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from filn_depot_registration import plan,queue

def fixture():
 return dict(exported_at=time.time(),errors=[],stations=[dict(id='S1',guid='a'*32,is_depot=True,position=dict(x=0,y=0,z=0))],vehicles=[dict(id='V1',key='vehicle one',route=[],driver=False,cargo=dict(amount=0),position=dict(x=0,y=0,z=0))])
class RegistrationTests(unittest.TestCase):
 def test_unique_pair(self):self.assertEqual(plan(fixture())[0]['home'],'a'*32)
 def test_existing_route_driver_cargo_or_different_floor_skipped(self):
  for change in [dict(route=['b'*32]),dict(driver=True),dict(cargo=dict(amount=1)),dict(position=dict(x=0,y=0,z=5))]:
   s=fixture();s['vehicles'][0].update(change);self.assertEqual(plan(s),[])
 def test_ambiguous_depots_and_duplicate_trucks_skipped(self):
  s=fixture();s['stations'].append(dict(s['stations'][0],guid='b'*32));self.assertEqual(plan(s),[])
  s=fixture();s['vehicles'].append(dict(s['vehicles'][0],id='V2',key='other'));self.assertEqual(plan(s),[])
 def test_stale_sampling_errors_rejected(self):
  for change in [dict(exported_at=0),dict(errors=['missing'])]:
   s=fixture();s.update(change)
   with self.assertRaises(ValueError):plan(s)
 def test_unrelated_station_sample_failure_does_not_block_native_rechecked_pair(self):
  s=fixture();s['errors']=[dict(kind='station',id='S99',error='filter conflict')]
  self.assertEqual(len(plan(s)),1)
 def test_queue_stable_identity_duplicate_and_completed_request(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d);r=queue(p,fixture());self.assertIn('a'*32+'\tvehicle one',(p/'depot-registration.tsv').read_text())
   with self.assertRaises(ValueError):queue(p,fixture())
   (p/'depot-registration-result.json').write_text(json.dumps(dict(request_id=r['request_id'],status='complete')))
   self.assertNotEqual(queue(p,fixture())['request_id'],r['request_id'])

"""保持BTのsource/policy/CAT byte結合だけを検証するhost fixture。"""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

REPO=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('bt_membership',REPO/'tools/ci/retained_bt_membership.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


class RetainedBT(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=REPO.parents[1]/'port/gsi/logs/ios-feasibility',prefix='bt-policy-host-')
        self.root=Path(self.temp.name);self.directory=self.root/'actual2018';self.directory.mkdir()
        files=[];hashes={};names={}
        for kind in ('inf','sys','cat'):
            name='bt.'+kind;p=self.directory/name;p.write_bytes(('fixture-'+kind).encode())
            files.append(dict(path=name,sha256=m.sha(p),bytes=p.stat().st_size));hashes[kind]=m.sha(p);names[kind]=name
        self.mp=self.directory/'manifest.json'
        self.mp.write_text(json.dumps(dict(source_commit='a'*40,version='2026.11.9.2018',signature_status='Valid',files=files)))
        self.p=dict(bt_first_install_execute=False,install_targets=['render'],stage_order=['render','bt'],first_install_mask=0,deferred_unbound_mask=0,
                    bt_first_install=dict(directory='missing-old2013'),baselines=dict(bt=dict(mode='retained',version_u64=2018,date_filetime=42,sys_sha256=hashes['sys'])),
                    packages=dict(bt=dict(directory='actual2018',manifest='actual2018/manifest.json',manifest_sha256=m.sha(self.mp),source_commit='a'*40,driver_version='2026.11.9.2018',version_u64=2018,date_filetime=42,names=names,hashes=hashes,extra_members=[])),
                    input_pins=[dict(path=p.relative_to(self.root).as_posix(),sha256=m.sha(p)) for p in [self.mp]+[self.directory/names[k] for k in ('inf','sys','cat')]])
        self.pp=self.root/'policy.json'

    def tearDown(self):self.temp.cleanup()

    def invoke(self,policy=None):
        self.pp.write_text(json.dumps(policy or self.p))
        return m.plan(self.root,self.pp)

    def test_missing_inactive_2013_is_not_a_dependency(self):
        plan=self.invoke()
        self.assertFalse((self.root/'missing-old2013').exists())
        self.assertEqual(plan['selection'],'policy.packages.bt')
        self.assertEqual(plan['catalog']['sha256'],self.p['packages']['bt']['hashes']['cat'])
        self.assertEqual(len(plan['members']),2)
        self.assertFalse(plan['Windows_CAT_membership_verified'])

    def test_active_or_mismatched_baseline_rejected(self):
        changes=[('bt_first_install_execute',True),('first_install_mask',2),('deferred_unbound_mask',2),('install_targets',['bt'])]
        for key,value in changes:
            with self.subTest(key=key):
                p=copy.deepcopy(self.p);p[key]=value
                with self.assertRaises(ValueError):self.invoke(p)
        for key,value in [('mode','selected'),('sys_sha256','0'*64),('version_u64',2013),('date_filetime',0)]:
            p=copy.deepcopy(self.p);p['baselines']['bt'][key]=value
            with self.assertRaises(ValueError):self.invoke(p)

    def test_catalog_manifest_and_member_pins_required(self):
        for row in self.p['input_pins']:
            p=copy.deepcopy(self.p);p['input_pins']=[v for v in p['input_pins'] if v['path']!=row['path']]
            with self.subTest(path=row['path']),self.assertRaises(ValueError):self.invoke(p)
        for kind in ('inf','sys','cat'):
            p=copy.deepcopy(self.p);p['packages']['bt']['hashes'][kind]='0'*64
            with self.assertRaises(ValueError):self.invoke(p)

    def test_changed_actual_byte_and_stale_plan_rejected(self):
        first=self.invoke();cat=self.directory/'bt.cat';cat.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.invoke()
        # policy/pinだけを新byteへ合わせてもsigned manifestに結合しない。
        p=copy.deepcopy(self.p);p['packages']['bt']['hashes']['cat']=m.sha(cat)
        for row in p['input_pins']:
            if row['path']=='actual2018/bt.cat':row['sha256']=m.sha(cat)
        with self.assertRaises(ValueError):self.invoke(p)
        self.assertNotEqual(first['catalog']['sha256'],m.sha(cat))

    def test_source_version_and_signing_report_not_relabeled(self):
        for key,value in [('source_commit','b'*40),('driver_version','2013')]:
            p=copy.deepcopy(self.p);p['packages']['bt'][key]=value
            with self.assertRaises(ValueError):self.invoke(p)

    def test_builder_retains_legacy_branch_and_delegates_render(self):
        src=(REPO/'tools/ci/build-combined200-service.ps1').read_text(encoding='utf-8-sig')
        block=src[src.index('  if($SignedRenderUpdate){\n   $btVerifier='):]
        retained,legacy=block.split('  }else{',1)
        self.assertIn('--verify-plan',retained)
        self.assertNotIn('bt_first_install',retained)
        self.assertIn('$bt=$policy.bt_first_install',legacy)


if __name__=='__main__':unittest.main()

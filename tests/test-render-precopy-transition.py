#!/usr/bin/env python3
"""実policyで、更新前UMDと更新後UMDの混同を再発させない。"""
import argparse,copy,importlib.util,json,sys
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--policy',type=Path,required=True);a=ap.parse_args()
p=Path(__file__).resolve().parents[1]/'tools/winpe/build-private-peripheral-batch-media.py'
sys.path.insert(0,str(p.parent))
s=importlib.util.spec_from_file_location('render_media_regression',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
policy=json.loads(a.policy.read_text(encoding='utf-8-sig'));before=copy.deepcopy(policy)
r=m.render_precopy_replacements(policy);key='Windows/System32/ge8300-umd.dll'
assert set(r)=={key} and r[key]['baseline']['sha256']!=r[key]['candidate']['sha256']
assert policy==before
for case in ('new-used-as-old','extra-firmware-transition','missing-old-proof'):
 q=copy.deepcopy(policy)
 if case=='new-used-as-old':q['baseline_witnesses'][key]['sha256']=q['preserved_files'][key]
 elif case=='extra-firmware-transition':
  x=next(x for x in q['packages']['render']['extra_members'] if x['name']=='ge8300-mips.fw');x['baseline_sha256']='0'*64
 else:del q['baseline_witnesses'][key]
 try:m.render_precopy_replacements(q)
 except ValueError:pass
 else:raise AssertionError(case+' admitted')
print('Real render policy: old/new UMD separated; tampered old proof and other transitions rejected')

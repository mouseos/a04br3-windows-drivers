"""Prevent an unfinished next-GPU scaffold from reaching executable generation."""
from pathlib import Path
import copy,importlib.util
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('draft142',R/'tools/recovery/prepare-peripheral-followup142.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
m=p.validate_draft(p.draft_contract())
assert m['install_targets']==['render'] and m['planned_reboots']==1 and m['planned_diagnostics']['gpu_last']
for field,value in [('normal_install_allowed',True),('execution_enabled',True),('gpu_client_execute',True),('gpu_client_count',1),('install_targets',['render','parent'])]:
 bad=copy.deepcopy(m);bad[field]=value
 try:p.validate_draft(bad)
 except ValueError:pass
 else:raise AssertionError(field+' enabled draft')
for entry in [p.load_map,p.generate,p.assert_paths,p.reject_arm]:
 try:entry(m)
 except ValueError as e:assert 'non-armable' in str(e)
 else:raise AssertionError('unfinished profile armed')
print('fresh142 draft identity/render-only/one-last-GPU/fail-closed: OK')

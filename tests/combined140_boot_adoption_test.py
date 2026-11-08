"""Reject failed139 boot even when a fresh-state rebind is authorized."""
import importlib.util,copy
from pathlib import Path
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('boot140',R/'tools/recovery/prepare-peripheral-followup140.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
c=p.boot_adoption_contract();assert c['adoption_status']=='main-artifacts-and-recovery-state-verified'
assert c['required_firmware_record_version']==3 and c['required_thermal_parent_build']==1104
assert c['matched_boot']['sha256'] not in c['rejected_boot_sha256']
original=p.boot_adoption_contract
candidate=copy.deepcopy(c);candidate['matched_boot']['sha256']=candidate['rejected_boot_sha256'][0];p.boot_adoption_contract=lambda:candidate
try:p.validate_boot_adoption(R,{'matched_boot':candidate['matched_boot']},lambda x:None)
except ValueError as e:assert 'must not be retried' in str(e)
else:raise AssertionError('failed139 boot accepted')
finally:p.boot_adoption_contract=original
assert c['matched_boot']['sha256'] not in (R/'tools/recovery/prepare-peripheral-followup140.py').read_text()
print('fresh140 single boot adoption / failed139 rejection: OK')

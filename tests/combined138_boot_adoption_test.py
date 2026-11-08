"""Failed139 boot adoption must stay blocked until a sealed replacement is selected."""
import importlib.util
from pathlib import Path
R=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('boot138',R/'tools/recovery/prepare-peripheral-followup138.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
c=p.boot_adoption_contract()
assert c['schema']==1 and c['adoption_status'].startswith('blocked-failed-run139')
assert c['runtime_success_claimed'] is False and c['failure']['Windows_unapplied'].startswith('inference only')
assert c['required_firmware_record_version']==3 and c['required_thermal_interface_version']==3 and c['required_thermal_parent_build']==1104
try:p.validate_boot_adoption(R,{},lambda x:(_ for _ in ()).throw(AssertionError('blocked adoption read private inputs')))
except ValueError as e:assert 'failed139 cannot be retried' in str(e)
else:raise AssertionError('failed boot accepted')
assert c['matched_boot']['sha256'] not in (R/'tools/recovery/prepare-peripheral-followup138.py').read_text()
print('single boot-pin contract / failed139 rejection / no assumed Windows state: OK')

original=p.boot_adoption_contract
candidate=dict(c);candidate['adoption_status']='main-artifacts-and-recovery-state-verified'
p.boot_adoption_contract=lambda:candidate
try:p.validate_boot_adoption(R,{'matched_boot':candidate['matched_boot']},lambda x:None)
except ValueError as e:assert 'must not be retried' in str(e)
else:raise AssertionError('same failed boot accepted after status update')
finally:p.boot_adoption_contract=original

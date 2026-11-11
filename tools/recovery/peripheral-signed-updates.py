#!/usr/bin/env python3
"""Shared200 package adoption inputs only; never generates a deployable profile/header."""
import argparse,copy,importlib.util,json,re
from pathlib import Path
HERE=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('updates_shared200',HERE/'prepare-peripheral-followup200.py')
shared=importlib.util.module_from_spec(sp);sp.loader.exec_module(shared)
require,read,sha,rooted=shared.require,shared.read,shared.sha,shared.rooted
LOCK=HERE/'peripheral-signed-updates-accepted.json'
SIGNER='BCBF6C456C6F0DD0818434763B189488FBF13570'
ROLES=('wifi','keypad')

def pinned(root,ref):
 require(isinstance(ref,dict) and set(ref)=={'path','sha256'} and re.fullmatch('[0-9a-f]{64}',ref['sha256']) and ref['sha256']!='0'*64,'Actual path/SHA required; pending rejected')
 p=rooted(root,ref['path']);require(sha(p)==ref['sha256'],'Input drift: '+ref['path']);return read(p)

def candidate_rows(root):
 """Return real manifests/role rows for shared200 TIMING.adopt, no flat receipt."""
 lock=read(LOCK);require(lock['schema']=='SIGNED_PERIPHERAL_UPDATES_ACCEPTED_V1' and set(lock['packages'])==set(ROLES),'Exact reviewed two-role lock')
 rows={}
 for role,x in lock['packages'].items():
  m=pinned(root,x['manifest']);review=pinned(root,x['main_review'])
  require(m['schema']==x['schema'] and m['source_commit']==x['compiled_source_commit'] and m['signed'] is True and m['version']==x['version'],'Actual signed schema/compile source/version')
  if role=='wifi':
   require(m['success'] is True and m['signer_thumbprint']==SIGNER and m['compiled_source_commit']==m['source_commit'] and m['signing_recipe_commit']==x['signing_source_commit'] and m['private_driver_baseline_required'] is False and m['non_signature_payload_unchanged'] is True and m['original_unsigned_manifest_preserved'] is True,'Public cold signed1033, not incremental1033')
   require(review['steps_exit_zero']==9 and review['non_signature_SYS_payload_unchanged'] is True and review['Windows_verify_CAT_membership_certificate_identity_steps_bound'] is True and review['compiled_source_commit']==m['source_commit'] and review['signing_recipe_commit']==m['signing_recipe_commit'],'Existing main cold-sign review')
   files=m['files']
  else:
   require(m['status']=='cross-built-WDK81-signed-catalog-verified-not-device-tested' and m['signer']==SIGNER and m['signing_source_commit']==x['signing_source_commit'] and m['metadata_source_commit']==x['metadata_source_commit'] and m['observation_source']==x['observation_source'] and m['functional_fix_claimed'] is False and m['device_tested'] is False,'Actual signed14 split provenance; raw deployable=false is not rewritten')
   require(review['manifest_archive_trio_hashsize_verified'] is True and review['non_signature_SYS_payload_identical'] is True and review['Windows_verify_and_CAT_membership_receipt_bound'] is True,'Existing main keypad review')
   files=m['outputs']
  require(len(files)==3 and {Path(f['path']).name for f in files}==set(x['names'].values()),'Exact trio only')
  for f in files:
   name=Path(f['path']).name;p=rooted(root,Path(x['directory'])/name)
   require(p.stat().st_size==f['bytes'] and sha(p)==f['sha256']==x['hashes'][p.suffix[1:]],'Actual signed trio hash/size')
  row=dict(directory=x['directory'],manifest=x['manifest']['path'],manifest_sha256=x['manifest']['sha256'],source_commit=m['source_commit'],driver_version=x['version'],driver_date=x['date'],names=x['names'])
  rows[role]=shared.base.package(root,row,role) # existing HWID/class/INF/source/trio guard
 return rows

def adopt(root,role,baseline):
 """Call inside existing shared200 TIMING.adopt; unrelated roles stay strict."""
 require(role in ROLES,'Only WiFi1033/keypad14 adopted by this source adapter')
 row=candidate_rows(root)[role]
 require(baseline['names']['sys']==row['names']['sys'],'No installed-path change')
 return row

def input_refs(root):
 lock=read(LOCK);rows=candidate_rows(root);refs=[]
 for role,x in lock['packages'].items():
  refs.extend([x['manifest'],x['main_review']])
  refs.extend(dict(path=(Path(rows[role]['directory'])/name).as_posix(),sha256=rows[role]['hashes'][kind]) for kind,name in rows[role]['names'].items())
 return refs

def validate_baseline(root,spec):
 """Use shared200's complete inventory contract, not a handpicked service subset."""
 require(spec['schema']=='SIGNED_PERIPHERAL_UPDATES_BASELINE_INPUTS_V1','Explicit baseline input contract')
 capture=spec['capture_number'];require(type(capture) is int and 1<=capture<=9999,'Actual capture number')
 service='MT8167CombinedFollowup'+str(capture)
 fm=pinned(root,spec['fresh']);n=pinned(root,spec['native']);c=pinned(root,spec['capture_contract']);prior=pinned(root,spec['template_policy'])
 require(fm['status']=='captured-unmounted' and fm['read_only'] is True and fm['unmounted'] is True and fm['errors']==[],'Actual completed fresh only')
 require(n['source_manifest_sha256']==spec['fresh']['sha256'] and n['recovery_boot_id']==fm['recovery_boot_id'] and n['capture_number']==capture and n['originals_unchanged'] is True and n['errors']==[] and n['registry_write_requested'] is False and n['full_service_capture_complete'] is True,'Same-fresh complete native inventory')
 require(all(n[k]==0 for k in ('load_rc','unload_rc','privilege_restore_error')) and n['new_service_name']==service and n['new_service']['present'] is False,'Original reader cleanup/new service absent')
 require(c['fresh_manifest_sha256']==spec['fresh']['sha256'] and c['native_receipt_sha256']==spec['native']['sha256'] and c['recovery_boot_id']==fm['recovery_boot_id'],'Same-fresh contract')
 for name in spec['additional_service_names']:
  require(name in n['old_services'] and n['old_services'][name]['present'] is True,'Additional old service must not be omitted')
 original={x['source_path']:x for x in fm['files']}
 for name,h in n['source_files'].items():require(original['Windows/System32/config/'+name]['sha256']==h,'Same SYSTEM trio')
 shared.CAPTURE=capture;shared.SERVICE=service
 from types import SimpleNamespace
 shared.TIMING=SimpleNamespace(contract_family='SIGNED_PERIPHERAL_UPDATES')
 values=shared.inventory_contract(c,n,prior)
 return dict(capture_number=capture,service=service,source=spec['native'],service_preservation=copy.deepcopy(n['old_services']),captured_registry_values=values,complete_service_set_preserved=True,new_service_absent=True,build_allowed=False,native_patch_allowed=False,device_operations_allowed=False)

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--workspace',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);ap.add_argument('--baseline-map',type=Path);a=ap.parse_args()
 root=a.workspace.resolve(strict=True);out=(root/a.out).resolve();require(out.is_relative_to(root) and not out.exists(),'New workspace-local preparation output')
 result=dict(schema='SIGNED_PERIPHERAL_UPDATES_PREPARATION_V1',packages=candidate_rows(root),input_refs=input_refs(root),install_targets=list(ROLES),install_mask=136,GPU=None,USB=None,final_policy_generated=False,build_allowed=False,native_patch_allowed=False,device_operations_allowed=False,required_macros=['A04_KEYPAD13_CHECKPOINT=1','A04_KEYPAD14_CHECKPOINT=1'])
 if a.baseline_map:result['baseline']=validate_baseline(root,read(rooted(root,a.baseline_map)))
 out.mkdir(parents=True);(out/'candidate-preparation.json').write_text(json.dumps(result,indent=2)+'\n')
 print('CANDIDATES_PREPARED_GPU_USB_FINAL_POLICY_PENDING_NO_HEADER_BUILD_OR_NATIVE')
if __name__=='__main__':main()

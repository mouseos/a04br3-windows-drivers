"""Thin379 transition over the retained231/200 builder; exact lock required."""
import copy,importlib.util,re
from pathlib import Path

def make(owner):
 f=owner;require,read,sha,rooted=(f[n] for n in ('require','read','sha','rooted'))
 def load(name):
  s=importlib.util.spec_from_file_location(name,Path(__file__).with_name(name+'.py'));m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
 native=load('followup231-vidpn379-schema');admission=load('gpu373_client_admission')
 def configure(lock):
  require(lock['adapter_id']==native.ADAPTER_ID and lock['mode']=='render-update','379 render-only adapter')
  s=f['configure_baseline']();s.PROFILE=s.base.PROFILE=f['PROFILE'];s.INSTALL=s.base.INSTALL=['render'];s.SOURCE=native.RECIPE;s.MANIFEST=lock['GPU_candidate']['sha256'];return s
 class Transition(f['Baseline231']):
  @property
  def install_targets(self):return ['render']
  def validate_manifest(self,g,r,spec):
   self.lock=f['accepted'](self.root);self.manifest,self.hashes=native.validate(self.root,rooted(self.root,self.lock['GPU_candidate']['path']),self.lock)
   require(g==self.manifest and spec['GPU368_manifest']==self.lock['GPU_candidate'] and spec['GPU368_handoff']==self.lock['main_review'],'exact379 shared-builder binding')
  def validate_output_review(self,r,h):require(h==self.hashes,'full379 hashes')
  def adopt(self,role,prior,originals,hashes):
   require(role=='render' and prior['driver_version']=='0.0.3.78' and prior['manifest_sha256']==native.BASE_MANIFEST,'actual selected378 to379 only')
   row=copy.deepcopy(prior);ref=self.lock['GPU_candidate']
   row.update(directory=Path(ref['path']).parent.as_posix(),manifest=ref['path'],manifest_sha256=ref['sha256'],source_commit=native.RECIPE,binary_source_commit=native.RECIPE,driver_version='0.0.3.79',driver_date='11/11/2026',artifact_review=self.lock['main_review'])
   for x in row['extra_members']:
    name='Windows/System32/'+x['installed'].replace('\\','/');require(originals[name]['sha256']==x['sha256']==hashes[x['name']],'379 retains UMD/FW');x['baseline_sha256']=x['sha256']
   return row
  def client(self,prior,originals,origin_sources,preserved,sources,witnesses,**unused):
   c=super().client(prior,originals,origin_sources,preserved,sources,witnesses,**unused)
   ref=self.lock['client_pair'];pair=read(rooted(self.root,ref['path']))
   require(set(pair)=={'schema','launcher_source_commit','launcher','evaluator'} and pair['schema']=='FOLLOWUP231_SIGNED_CLIENT_PAIR_V1','exact signed settle pair shape')
   l=admission._signed_leaf(self.root,pair['launcher']);e=admission._signed_leaf(self.root,pair['evaluator'])
   require(e['phase']=='evaluator' and e['runtime_source_commit']==native.CLIENT and pair['evaluator']['file']['sha256']==native.EVALUATOR,'fixed bdc6 signed evaluator, never recompile')
   require(re.fullmatch('[0-9a-f]{40}',pair['launcher_source_commit']) and l['phase']=='launcher' and l['runtime_source_commit']==pair['launcher_source_commit'] and l['evaluator_source_commit']==native.CLIENT and l['evaluator_sha256']==native.EVALUATOR,'formal exact-evaluator launcher binding')
   directory=Path(pair['launcher']['file']['path']).parent
   require(Path(pair['evaluator']['file']['path']).parent==directory,'one signed pair directory')
   c.update(directory=directory.as_posix(),sha256=pair['launcher']['file']['sha256'],phase=1,dependencies=[dict(exe=admission.EVALUATOR,sha256=native.EVALUATOR)])
   self.replacements={}
   for role,name in (('launcher',admission.LAUNCHER),('evaluator',admission.EVALUATOR)):
    leaf=pair[role];require(Path(leaf['file']['path']).name==name,'stable signed pair filenames')
    installed='Windows/System32/'+name;baseline=dict(path=origin_sources[installed],sha256=originals[installed]['sha256'])
    require(baseline['sha256']!=leaf['file']['sha256'],'explicit changed pair only')
    row=dict(baseline=baseline,candidate=leaf['file'],manifest=leaf['manifest'],import_proof=leaf['import_proof']);self.replacements[installed]=row
    witnesses[installed]=baseline;preserved[installed]=leaf['file']['sha256'];sources[installed]=leaf['file']['path']
    self.client_references.extend(leaf[k] for k in ('file','manifest','import_proof'))
   self.client_references.append(ref)
   return c
  def finish(self,p,spec,n,pin,pin_file):
   super().finish(p,spec,n,pin,pin_file)
   p['GPU379_manifest']=p.pop('GPU378_manifest');p['GPU379_handoff']=p.pop('GPU378_handoff')
   for k in ('followup231_pending','baseline_only'):p.pop(k,None)
   if 'gpu378_contract' in p:p['history_gpu378_contract_reference_only']=p.pop('gpu378_contract')
   p.update(profile=f['PROFILE'],draft=False,normal_install_allowed=True,main_execution_approved=True,diagnostic_only=False,driver_updates=True,planned_reboots=1)
   p['display_transition'].update(DIF_order=['render'],old_DOD_retirement='378 to379 trace only; transport372/keypad13/WiFi1031/battery18/UMD/FW/boot retained')
   p['development_display_on']=dict(macro='A04_DEVELOPMENT_DISPLAY_ON',value=1,phase=1,before_run_clients=True,after_readback=True,buttons_fix_claimed=False)
   p['diagnostic_clients'][1]['arguments']=['"C:\\Windows\\System32\\'+f['SERVICE']+'\\logs\\global-diag379"'];p['globaldiag_contract']['prefix']=p['diagnostic_clients'][1]['arguments'][0]
   p['standard_present_contract'].update(driver_package_source_commit=native.RECIPE,driver_signed_manifest_sha256=self.lock['GPU_candidate']['sha256'])
   p['followup231_contract']=dict(schema='FOLLOWUP231_TRANSITION_V1',input=spec['_input_ref'],mode='render-update',fresh=231,run=232,post_capture=233,install_mask=1024,accepted_lock_sha256=sha(f['ACCEPTED']),terminal_windows_hold=True,hardware_success_claimed=False,client_replacements=self.replacements,trace_schema='GE8300_VIDPN_CALLBACK_TRACE_V1')
   for key in ('GPU_candidate','main_review','client_pair'):pin(self.lock[key])
 class Adapter:
  @staticmethod
  def load_map(path,root,lock):
   spec=read(path)
   require(set(spec)=={'schema','profile','capture_number','hardware_owner','main_execution_approved','baseline_inputs','mode','GPU_candidate','client_pair','filter_root_transitions','install_targets','planned_reboots'} and spec['schema']=='FOLLOWUP231_FINALIZE_INPUTS_V1','explicit231 finalize input shape')
   require(spec['profile']==f['PROFILE'] and spec['capture_number']==231 and spec['hardware_owner']=='main' and spec['main_execution_approved'] is True and spec['mode']=='render-update' and spec['install_targets']==['render'] and spec['planned_reboots']==1 and spec['filter_root_transitions']==[],'minimal379 render transition; no filter exemption')
   require(spec['GPU_candidate']==lock['GPU_candidate'] and spec['client_pair']==lock['client_pair'],'exact accepted candidates')
   br=spec['baseline_inputs'];bp=rooted(root,br['path']);require(set(br)=={'path','sha256'} and sha(bp)==br['sha256'],'actual231 baseline input binding')
   f['baseline'](bp,root) # Existing full actual231 verification; no output or hive load.
   baseline=read(bp);current=copy.deepcopy(baseline);current.update(profile=f['PROFILE'],install_targets=['render'],main_execution_approved=True,GPU368_manifest=lock['GPU_candidate'],GPU368_handoff=lock['main_review'],_input_ref=dict(path=path.relative_to(root).as_posix(),sha256=sha(path)))
   s=configure(lock);s.TIMING=Transition(root,current,{})
   p=s.build_policy(root,current)
   pins={x['path']:x['sha256'] for x in p['input_pins']}
   for r in read(rooted(root,baseline['template_policy']['path']))['input_pins']:
    require(sha(rooted(root,r['path']))==r['sha256'],'historical input closure');pins[r['path']]=r['sha256']
   for r in (br,current['_input_ref']):pins[r['path']]=r['sha256']
   p['input_pins']=[dict(path=k,sha256=v) for k,v in sorted(pins.items())]
   return p
  @staticmethod
  def generate(p,lock):
   require(p['followup231_contract']['accepted_lock_sha256']==sha(f['ACCEPTED']) and p['install_targets']==['render'] and not p.get('filter_root_transitions'),'379 exact source guard')
   return '#define A04_KEYPAD13_CHECKPOINT 1\n#define A04_DISPLAY_IDLE_BEFORE_CLIENTS 1\n#define A04_VIDPN_CALLBACK_TRACE 1\n'+configure(lock).generate(p)
  @staticmethod
  def validate_runtime_policy(root,p,lock):require(p==Adapter.load_map(rooted(root,p['followup231_contract']['input']['path']),root,lock),'full actual231/379 policy regeneration equality')
 return native.ADAPTER_ID,Adapter

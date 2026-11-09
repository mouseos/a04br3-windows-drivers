#!/usr/bin/env python3
"""CAT-only recovery. Recursive Inf2Cat runs before signing either final CAT."""
from pathlib import Path
import json,hashlib,subprocess,argparse,zipfile,shutil
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--expected-manifest',required=True);p.add_argument('--provider-inf-source',type=Path);p.add_argument('--provider-inf-sha256');a=p.parse_args();t=a.task_root;b=t/'build';pkg=b/'package357';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();manifest=pkg/'signed-manifest.json';assert sha(manifest)==a.expected_manifest;m=json.loads(manifest.read_text(encoding='utf-8-sig'));backup=t/('outputs357-before-'+a.expected_manifest[:12]+'.zip')
if not backup.exists():shutil.copyfile(t/'outputs357.zip',backup)
with zipfile.ZipFile(backup)as z:
 assert z.testzip()is None
 assert hashlib.sha256(z.read('package357/signed-manifest.json')).hexdigest()==a.expected_manifest
 before={n:hashlib.sha256(z.read(n)).hexdigest()for n in z.namelist()if not n.endswith('/')}

def run(c,n):
 r=subprocess.run([str(x)for x in c],capture_output=True,text=True);(b/'logs'/n).write_text(r.stdout+r.stderr);r.check_returncode()
sign='C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe';thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';ds=[(pkg,'ge8300-render.cat'),(pkg/'display-transport','mt8167-display-transport.cat')]
if a.provider_inf_source:
 assert sha(a.provider_inf_source)==a.provider_inf_sha256
 shutil.copyfile(a.provider_inf_source,pkg/'display-transport/mt8167-display-transport.inf')
# Inf2Cat traverses subdirectories: generate everything, then sign everything.
run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'final-catalog-generation.log')
for d,n in ds:run([sign,'sign','/fd','SHA256','/sha1',thumb,d/n],n+'.final-sign.log')
for d,n in ds:
 run([sign,'verify','/pa',d/n],n+'.final-verify.log')
 for f in (['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']if d==pkg else['mt8167-display-transport.inf','mt8167-display-transport.sys']):run([sign,'verify','/pa','/c',d/n,d/f],f+'.final-CAT.log')
changed=[n for n,h in before.items()if sha(b/n)!=h];allowed={'package357/ge8300-render.cat','package357/display-transport/mt8167-display-transport.cat'};allowed|=({'package357/display-transport/mt8167-display-transport.inf'}if a.provider_inf_source else set());assert set(changed)<=allowed,changed
m['package_source_commit']=a.source_commit;m['packaging_source_commit']=a.source_commit;m['compile_source_commit']='efaf1008add44565f6c5fc4597e67804b22570fb';m['diagnostic_registry_parent_key_only']=bool(a.provider_inf_source);m['packaging_only_recovery']={'compile':0,'link':0,'SYS_resign':0,'before_manifest_sha256':a.expected_manifest,'changed_existing_files':changed,'retained_files':len(before)-len(changed)}
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in m['outputs']];m.pop('build_manifest_sha256',None);(b/'build-manifest357.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(b/'build-manifest357.json',pkg/'start357-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'start357-build-manifest.json');manifest.write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs357.zip','w',zipfile.ZIP_DEFLATED)as z:
 for q in b.rglob('*'):
  if q.is_file():z.write(q,q.relative_to(b).as_posix())
print('357_CAT_ONLY_RECOVERED_COMPILE0_LINK0_SYSUNCHANGED',sha(manifest))

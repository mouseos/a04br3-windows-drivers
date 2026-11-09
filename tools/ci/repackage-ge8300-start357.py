#!/usr/bin/env python3
"""CAT-only recovery. Recursive Inf2Cat runs before signing either final CAT."""
from pathlib import Path
import json,hashlib,subprocess,argparse,zipfile,shutil
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--expected-manifest',required=True);a=p.parse_args();t=a.task_root;b=t/'build';pkg=b/'package357';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();manifest=pkg/'signed-manifest.json';assert sha(manifest)==a.expected_manifest;m=json.loads(manifest.read_text(encoding='utf-8-sig'));before={str(p.relative_to(b)):sha(p)for p in b.rglob('*')if p.is_file()};assert not(t/'outputs357-nonadopted-cat.zip').exists();shutil.copyfile(t/'outputs357.zip',t/'outputs357-nonadopted-cat.zip')
def run(c,n):
 r=subprocess.run([str(x)for x in c],capture_output=True,text=True);(b/'logs'/n).write_text(r.stdout+r.stderr);r.check_returncode()
sign='C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe';thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';ds=[(pkg,'ge8300-render.cat'),(pkg/'display-transport','mt8167-display-transport.cat')]
# Inf2Cat traverses subdirectories: generate everything, then sign everything.
run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'final-catalog-generation.log')
for d,n in ds:run([sign,'sign','/fd','SHA256','/sha1',thumb,d/n],n+'.final-sign.log')
for d,n in ds:
 run([sign,'verify','/pa',d/n],n+'.final-verify.log')
 for f in (['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']if d==pkg else['mt8167-display-transport.inf','mt8167-display-transport.sys']):run([sign,'verify','/pa','/c',d/n,d/f],f+'.final-CAT.log')
changed=[n for n,h in before.items()if sha(b/n)!=h];allowed={'package357/ge8300-render.cat','package357/display-transport/mt8167-display-transport.cat'};assert set(changed)<=allowed
m['package_source_commit']=a.source_commit;m['packaging_source_commit']=a.source_commit;m['compile_source_commit']='efaf1008add44565f6c5fc4597e67804b22570fb';m['packaging_only_recovery']={'compile':0,'link':0,'SYS_resign':0,'before_manifest_sha256':a.expected_manifest,'changed_existing_files':changed,'retained_files':len(before)-len(changed)}
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in m['outputs']];m.pop('build_manifest_sha256',None);(b/'build-manifest357.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(b/'build-manifest357.json',pkg/'start357-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'start357-build-manifest.json');manifest.write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs357.zip','w',zipfile.ZIP_DEFLATED)as z:
 for q in b.rglob('*'):
  if q.is_file():z.write(q,q.relative_to(b).as_posix())
print('357_CAT_ONLY_RECOVERED_COMPILE0_LINK0_SYSUNCHANGED',sha(manifest))

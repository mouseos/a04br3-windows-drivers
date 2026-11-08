#!/usr/bin/env python3
"""Execute a pinned real-include delta plan; retain exact prior ARM COFF objects.
Produces an archive/receipt only. Final SYS link/sign/CAT/runtime completion is separate.
"""
import argparse, hashlib, json, re, struct, subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def checked_file(p,h):
    if sha(p)!=h:raise ValueError('Pinned input mismatch: '+str(p))
def execute(root,plan_path,plan_hash,base_archive,receipt_path,receipt_hash,clang,ar,ar_hash,out):
    checked_file(plan_path,plan_hash);checked_file(receipt_path,receipt_hash);checked_file(ar,ar_hash)
    plan=json.loads(plan_path.read_text());prior=json.loads(receipt_path.read_text())
    if plan['schema']!='ge8300-actual-include-incremental-plan-v1':raise ValueError('Actual-include plan required')
    root=root.resolve(strict=True)
    checked_file(clang,plan['clang_sha256']);checked_file(base_archive,prior['archive_sha256'])
    if plan['base_commit']!=prior['source_commit']:raise ValueError('Archive source is not plan base')
    if prior['clang_sha256']!=plan['clang_sha256']:raise ValueError('Prior object compiler provenance changed')
    head=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    if head!=plan['source_commit']:raise ValueError('Source HEAD changed after dependency scan')
    if subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no'],text=True).strip():raise ValueError('Tracked source must be clean')
    members=[x['name'] for x in prior['members']];pinmap={x['name']:x['sha256'] for x in prior['members']}
    if len(members)!=len(pinmap) or not all(re.fullmatch(r'[A-Za-z0-9_.-]+[.]obj',x) for x in members):raise ValueError('Unique flat baseline objects required')
    objects=[u['object'] for u in plan['units']]
    if len(objects)!=len(set(objects)) or not all(re.fullmatch(r'[A-Za-z0-9_.-]+[.]obj',x) for x in objects):raise ValueError('Unique flat output objects required')
    if sorted(plan['removed_objects'])!=sorted(set(members)-set(objects)):raise ValueError('Unreviewed dropped archive objects')
    oldunits={u['object']:u for u in prior['units']}
    for u in plan['units']:
        old=oldunits.get(u['object'])
        if not u['rebuild'] and (not old or old['source']!=u['source'] or old['arguments']!=u['arguments']):raise ValueError('Retained object recipe lacks exact baseline proof')
        if not u['rebuild']:
            current_external={x['path']:x['sha256'] for x in u['dependencies'] if x['scope']=='toolchain'}
            prior_external={x['path']:x['sha256'] for x in old.get('dependencies',[]) if x['scope']=='toolchain'}
            if current_external!=prior_external:raise ValueError('Retained external dependency provenance mismatch')
        for d in u['dependencies']:
            p=root/d['path'] if d['scope']=='source' else Path(d['path'])
            if d['scope']=='source' and not p.resolve(strict=True).is_relative_to(root):raise ValueError('Dependency escaped source root')
            checked_file(p,d['sha256'])
    if out.exists():raise ValueError('New output required')
    out.mkdir(parents=True);before=out/'before';before.mkdir();objdir=out/'objects';objdir.mkdir()
    actual=subprocess.check_output([str(ar),'t',str(base_archive)],text=True).splitlines()
    if actual!=members:raise ValueError('Baseline archive order/member mismatch')
    subprocess.run([str(ar),'x','--output='+str(before),str(base_archive)],check=True)
    for name,h in pinmap.items():checked_file(before/name,h)
    rows=[]
    for u in plan['units']:
        target=objdir/u['object']
        if u['rebuild']:
            source=(root/u['source']).resolve(strict=True)
            if not source.is_relative_to(root):raise ValueError('Source outside checkout')
            args=[a.replace('${SOURCE_ROOT}',str(root)) for a in u['arguments']]
            command=[str(clang),*args,'-c',str(source),'-o',str(target)]
            result=subprocess.run(command,cwd=root,capture_output=True,text=True)
            (out/(u['object']+'.compile.log')).write_text(result.stdout+result.stderr)
            if result.returncode:raise ValueError('Actual ARM compile failed: '+u['source'])
        else:
            target.write_bytes((before/u['object']).read_bytes())
        data=target.read_bytes()
        if len(data)<20 or struct.unpack_from('<H',data)[0]!=0x1c4:raise ValueError('Actual ARMNT COFF object required: '+u['object'])
        h=sha(target)
        if not u['rebuild'] and h!=pinmap[u['object']]:raise ValueError('Retained object bytes changed')
        rows.append({'name':u['object'],'sha256':h,'compiled':u['rebuild'],'before_sha256':pinmap.get(u['object'])})
    archive=out/'runtime-delta.lib'
    subprocess.run([str(ar),'rcsD',str(archive),*[str(objdir/name) for name in objects]],check=True)
    if subprocess.check_output([str(ar),'t',str(archive)],text=True).splitlines()!=objects:raise ValueError('Output archive map mismatch')
    result={'schema':'ge8300-runtime-delta-archive-v1','source_commit':head,'base_commit':plan['base_commit'],'archive_sha256':sha(archive),'clang_sha256':plan['clang_sha256'],'llvm_ar_sha256':ar_hash,'members':rows,'units':[{k:u[k] for k in ['source','object','arguments','dependencies']} for u in plan['units']],'compiled_count':sum(x['compiled'] for x in rows),'retained_count':sum(not x['compiled'] for x in rows),'removed_objects':plan['removed_objects'],'signed':False,'linked_SYS':False,'runtime_complete':False,'device_verified':False}
    (out/'build-manifest.json').write_text(json.dumps(result,indent=2)+'\n');return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--plan',type=Path,required=True);p.add_argument('--plan-sha256',required=True);p.add_argument('--baseline-archive',type=Path,required=True);p.add_argument('--baseline-receipt',type=Path,required=True);p.add_argument('--baseline-receipt-sha256',required=True);p.add_argument('--clang',type=Path,required=True);p.add_argument('--llvm-ar',type=Path,required=True);p.add_argument('--llvm-ar-sha256',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    m=execute(a.source_root,a.plan,a.plan_sha256,a.baseline_archive,a.baseline_receipt,a.baseline_receipt_sha256,a.clang,a.llvm_ar,a.llvm_ar_sha256,a.out);print(json.dumps({'compiled':m['compiled_count'],'retained':m['retained_count'],'signed':False,'runtime_complete':False}))
if __name__=='__main__':main()

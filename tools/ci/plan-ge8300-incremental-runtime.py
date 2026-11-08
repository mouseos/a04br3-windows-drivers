#!/usr/bin/env python3
"""Plan required TU rebuilds from real Clang include dependencies and fixed Git base.
Does not compile, sign, or assert runtime completion. Missing external pins fail closed.
"""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

def sha(data): return hashlib.sha256(data).hexdigest()

def make_dependencies(text):
    text=text.replace('\\\r\n','').replace('\\\n','')
    prefix='__gpu_dependency_target:'
    if not text.startswith(prefix): raise ValueError('Unexpected Clang dependency target')
    text=text[len(prefix):];tokens=[];word='';i=0
    while i<len(text):
        c=text[i]
        if c=='\\' and i+1<len(text) and (text[i+1].isspace() or text[i+1] in '\\#$'):
            word+=text[i+1];i+=2;continue
        if c.isspace():
            if word:tokens.append(word);word=''
        else:word+=c
        i+=1
    if word:tokens.append(word)
    if not tokens:raise ValueError('Empty dependency closure')
    return tokens

def load_units(path):
    data=json.loads(path.read_text(encoding='utf-8-sig'))
    units=data['compiled_units'] if isinstance(data,dict) else data
    seen=set();sources=set()
    for u in units:
        if not re.fullmatch(r'[A-Za-z0-9_.-]+[.]obj',u['object']):raise ValueError('Flat COFF object name required')
        if u['object'] in seen or u['source'] in sources:raise ValueError('Duplicate source/object identity')
        if Path(u['source']).is_absolute() or '..' in Path(u['source']).parts:raise ValueError('Source outside checkout')
        if not isinstance(u['arguments'],list) or not all(isinstance(x,str) for x in u['arguments']):raise ValueError('Explicit arguments required')
        # The recipe supplies flags only. Avoid preprocessing output overrides and side effects.
        for flag in u['arguments']:
            if flag in ('-c','-o','-E','-M','-MD','-MMD','-MF','-MT','-MQ') or flag.startswith(('@','-fplugin','-Xclang')):raise ValueError('Compile arguments must contain trusted flags only')
        seen.add(u['object']);sources.add(u['source'])
    return units

def plan(root,base,current,prior,clang,clang_hash,external):
    root=root.resolve(strict=True);clang=clang.resolve(strict=True)
    if sha(clang.read_bytes())!=clang_hash:raise ValueError('Clang executable pin mismatch')
    resolved=subprocess.check_output(['git','-C',str(root),'rev-parse','--verify',base+'^{commit}'],text=True).strip()
    if subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no'],text=True).strip():raise ValueError('Tracked source must be clean')
    pins={str(Path(x['path']).resolve(strict=True)):x['sha256'] for x in external}
    # Verify all supplied external toolchain files, even when no TU uses one yet.
    for p,h in pins.items():
        if sha(Path(p).read_bytes())!=h:raise ValueError('External toolchain input pin mismatch: '+p)
    old={u['object']:u for u in prior};rows=[]
    for u in current:
        source=(root/u['source']).resolve(strict=True)
        if not source.is_relative_to(root):raise ValueError('Symlink source escaped checkout')
        flags=[x.replace('${SOURCE_ROOT}',str(root)) for x in u['arguments']]
        cmd=[str(clang),*flags,'-M','-MT','__gpu_dependency_target',str(source)]
        result=subprocess.run(cmd,cwd=root,capture_output=True,text=True)
        if result.returncode:raise ValueError('Actual Clang dependency scan failed for '+u['source']+'\n'+result.stderr)
        deps=[];reasons=[]
        for token in make_dependencies(result.stdout):
            p=Path(token)
            if not p.is_absolute():p=root/p
            p=p.resolve(strict=True);h=sha(p.read_bytes())
            if p.is_relative_to(root):
                rel=p.relative_to(root).as_posix()
                previous=subprocess.run(['git','-C',str(root),'cat-file','blob',resolved+':'+rel],capture_output=True)
                before=sha(previous.stdout) if previous.returncode==0 else None
                if h!=before:reasons.append('changed_dependency:'+rel)
                deps.append({'path':rel,'scope':'source','sha256':h,'base_sha256':before})
            else:
                if pins.get(str(p))!=h:raise ValueError('Unpinned actual external include: '+str(p))
                deps.append({'path':str(p),'scope':'toolchain','sha256':h})
        if not any(x['scope']=='source' and x['path']==u['source'] for x in deps):raise ValueError('TU itself absent from dependency closure')
        before=old.get(u['object'])
        if before is None:reasons.append('new_object')
        elif before['source']!=u['source'] or before['arguments']!=u['arguments']:reasons.append('source_or_codegen_flags_changed')
        external_now={x['path']:x['sha256'] for x in deps if x['scope']=='toolchain'}
        external_before={x['path']:x['sha256'] for x in (before or {}).get('dependencies',[]) if x['scope']=='toolchain'}
        if external_now!=external_before:reasons.append('external_input_provenance_changed_or_missing')
        rows.append({'source':u['source'],'object':u['object'],'arguments':u['arguments'],'rebuild':bool(reasons),'reasons':reasons,'dependencies':deps,'dependency_command':cmd})
    return {'schema':'ge8300-actual-include-incremental-plan-v1','base_commit':resolved,'source_commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'clang_sha256':clang_hash,'units':rows,'rebuild_objects':[x['object'] for x in rows if x['rebuild']],'retained_objects':[x['object'] for x in rows if not x['rebuild']],'removed_objects':sorted(set(old)-{x['object'] for x in rows}),'compiled':False,'signed':False,'runtime_complete':False,'device_verified':False,'object_reuse_conditions':'Plan only: build must separately pin old object/archive bytes, tools, link inputs and final runtime closure. Include/flags equality alone is not an object-signature receipt.'}

def main():
    p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--base-commit',required=True);p.add_argument('--current-units',type=Path,required=True);p.add_argument('--prior-units',type=Path,required=True);p.add_argument('--clang',type=Path,required=True);p.add_argument('--clang-sha256',required=True);p.add_argument('--external-pins',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    if a.out.exists():raise ValueError('New output required; previous plan is preserved')
    result=plan(a.source_root,a.base_commit,load_units(a.current_units),load_units(a.prior_units),a.clang,a.clang_sha256,json.loads(a.external_pins.read_text(encoding='utf-8-sig')))
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'units':len(result['units']),'rebuild_objects':result['rebuild_objects'],'removed_objects':result['removed_objects'],'compiled':False}))
if __name__=='__main__':main()

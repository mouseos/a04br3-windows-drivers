"""Present compatibility probes are explicitly NOT historical include evidence."""
import concurrent.futures,json,re,subprocess,time,os
from pathlib import Path
from ge8300_compiler_cache import dependencies

def present_compatibility(root,baseline,out,spec,sha,jobs,cl):
    out.mkdir();oldroot=baseline.parents[1].resolve();root=root.resolve()
    def key(p):return str(Path(p).resolve()).replace('\\','/').lower()
    oldprefix=key(oldroot)+'/'
    def relative_old(p):
        k=key(p)
        return k[len(oldprefix):] if k.startswith(oldprefix) else None
    def rebind(p):
        rel=relative_old(p)
        # Retained generated inputs stay at the frozen original artifact path.
        return root/rel if rel and not rel.startswith('build/') else Path(p)
    def flags(args):
        result=[];i=0
        while i<len(args):
            a=args[i]
            if a in ('-c','-o','-MF','-MT','-MQ'):i+=2;continue
            if a in ('-MD','-MMD'):i+=1;continue
            if a=='-I':result.extend(['-I',str(rebind(args[i+1]))]);i+=2;continue
            if a.startswith('-I'):a='-I'+str(rebind(a[2:]))
            result.append(a);i+=1
        return result
    compiler=next(x for x in spec['baseline_tools'] if x['name']=='clang')
    exe=re.sub(r'\\+',r'\\',compiler['path'])
    units=[];native=json.loads((baseline/'native/build-manifest.json').read_text(encoding='utf-8-sig'))
    for command in native['commands']:
        if command['tool']!='clang':continue
        a=command['arguments'];source=Path(a[a.index('-c')+1]);obj=Path(a[a.index('-o')+1]);dep=baseline/'native/parallel-compile-logs'/(obj.stem+'.d')
        units.append({'identity':'native-'+obj.stem,'source':source,'object':obj,'flags':flags(a),'original_trace':dep if dep.exists() else None,'scope':'native'})
    # The real direct-I/O receipts bind the module/UMD arguments and output path.
    for relative in spec['direct_compile_receipts']:
        d=json.loads((baseline/relative).read_text(encoding='utf-8-sig'));a=d['arguments'];source=Path(a[a.index('-c')+1]);obj=Path(a[a.index('-o')+1]);rel=relative_old(obj)
        if obj.name in spec['changed_objects'] and rel and rel.startswith('build/dwm-source/modules/'):continue
        units.append({'identity':('module-' if '/modules/' in key(obj) else 'umd-')+obj.stem,'source':source,'object':obj,'flags':flags(a),'original_trace':None,'scope':'module-or-UMD'})
    units.append({'identity':'module-mdl-pin','source':oldroot/'drivers/gpu/mt8167/wddm-render/mdl_pin.c','object':baseline/'modules/mdl_pin.obj','flags':spec['mdl_pin_arguments'],'original_trace':None,'scope':'module-MSVC-SEH','tool':'msvc'})
    known_external={key(re.sub(r'\\+',r'\\',r['path'])):r['sha256'].lower() for r in native['wdk_inputs']}
    recorded_native=0
    def probe(u):
        start=time.monotonic();trace=u['original_trace'];command=None
        if trace:
            raw=trace;historical=True
        else:
            raw=out/(u['identity']+'.current-probe.d');err=raw.with_suffix('.stderr.log')
            command=([str(cl),*u['flags'],'/E','/showIncludes',str(rebind(u['source']))] if u.get('tool')=='msvc' else [exe,*u['flags'],'-M','-MT','ge8300-unit',str(rebind(u['source']))])
            with raw.open('wb') as so,err.open('wb') as se:p=subprocess.run(command,stdout=so,stderr=se,cwd=root,env=dict(os.environ,VSLANG="1033",TEMP=str(out),TMP=str(out)))
            if p.returncode:raise ValueError('Current compatibility probe failed: '+u['identity']+'; '+str(err))
            historical=False
        deps=([str(rebind(u['source'])),*re.findall(r'^Note: including file:\s*(.+)$',raw.read_text(errors='replace'),re.M)] if u.get('tool')=='msvc' else dependencies(raw))
        if u.get('tool')=='msvc' and len(deps)<2:raise ValueError('MSVC current include probe is empty')
        source_rows=[];external=[];changed=[]
        for token in deps:
            old=Path(token);p=rebind(old);rel=relative_old(old)
            # A current-probe path points at the new source root instead.
            if p.resolve().is_relative_to(root):
                rel=p.resolve().relative_to(root).as_posix();digest=sha(p);before=spec['baseline_repository_files'].get(rel)
                source_rows.append({'path':rel,'current_sha256':digest,'baseline_source_snapshot_sha256':before})
                if before!=digest:changed.append(rel)
            elif rel and rel.startswith('build/'):
                name=Path(old).resolve().relative_to(baseline).as_posix()
                if spec['baseline_files'].get(name)!=sha(old):raise ValueError('Unpinned retained generated input '+str(old))
                source_rows.append({'retained_generated_path':str(old),'sha256':sha(old)})
            else:
                digest=sha(p);pin=known_external.get(key(p))
                if pin and pin!=digest:raise ValueError('Recorded external include changed '+str(p))
                external.append({'path':str(p.resolve()),'current_sha256':digest,'historical_hash_recorded':pin is not None,'recorded_hash':pin})
        if not source_rows:raise ValueError('Missing owned include graph '+u['identity'])
        if changed:raise ValueError('Additional dependent TU requires a new rebuild plan: '+u['identity']+' '+','.join(changed))
        return {'identity':u['identity'],'scope':u['scope'],'original_compile_time_trace':historical,'trace_sha256':sha(raw),'current_probe_command':command,'object':str(u['object']),'object_sha256':sha(u['object']),'source_dependencies':source_rows,'external_dependencies':external,'elapsed_seconds':time.monotonic()-start,'strict_historical_include_closure_claimed':False}
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:rows=list(pool.map(probe,units))
    result={'schema':'dwm372-present-compatibility-v1','native_objects':67,'native_compile_time_traces':38,'native_historical_trace_absent':29,'historical_external_include_completeness_claimed':False,'probes_are_current_compatibility_only':True,'compiled_objects':0,'jobs':jobs,'units':rows,'success':True}
    if len([r for r in rows if r['scope']=='native'])!=67 or sum(r['original_compile_time_trace'] for r in rows if r['scope']=='native')!=38:raise ValueError('Native67/trace38 provenance changed')
    (out/'present-compatibility.json').write_text(json.dumps(result,indent=2)+'\n');return result

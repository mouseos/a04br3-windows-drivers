#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""固定PCO sourceとtarget中立生成headerからARM32 COFF archiveを作る。最終DLL linkは別工程。"""
import argparse, concurrent.futures, hashlib, json, os, re, shlex, shutil, subprocess
from pathlib import Path

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--reference-manifest',type=Path,required=True)
    ap.add_argument('--windows-headers',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--reuse-directory',type=Path)
    ap.add_argument('--msvc-sdk',type=Path)
    ap.add_argument('--msvc-vfs',type=Path)
    ap.add_argument('--component',choices=('pco','nir','util','support'),default='pco')
    a=ap.parse_args();here=Path(__file__).resolve().parent
    root=next(p for p in here.parents if (p/'windows-rt/AGENTS.md').exists())
    out=a.output.resolve()
    if not out.is_relative_to(root):raise SystemExit('output outside project')
    ref=json.loads(a.reference_manifest.read_text());build=Path(ref['build'])
    all_rows=json.loads((build/'compile_commands.json').read_text())
    pco_rows=[x for x in all_rows if x.get('output','').startswith('src/imagination/pco/libpowervr_compiler.a.p/')]
    mesa=(build/next(x['file'] for x in pco_rows if x['file'].endswith('/pco_nir.c'))).resolve().parents[3]
    if a.component=='pco':rows=pco_rows
    elif a.component=='nir':rows=[x for x in all_rows if x.get('output','').startswith(('src/compiler/nir/libnir.a.p/','src/compiler/libcompiler.a.p/'))]
    elif a.component=='support':
        template=pco_rows[0]
        rows=[dict(template,file=str(mesa/'src/util/blake3'/name),output=name) for name in ('blake3.c','blake3_dispatch.c','blake3_portable.c')]
        if a.msvc_sdk:rows.append(dict(template,file=str(here/'crt_stdio.cpp'),output='crt_stdio'))
    else:
        rows=[x for x in all_rows if x.get('output','').startswith(('src/util/libmesa_util.a.p/','src/c11/impl/libmesa_util_c11.a.p/')) and x['file'].endswith('.c') and not x['file'].endswith('/threads_posix.c')]
        rows.append(dict(rows[-1],file=str(mesa/'src/c11/impl/threads_win32.c'),output='threads_win32'))
        if a.msvc_sdk:
            rows.append(next(x for x in all_rows if x['file'].endswith('/u_qsort.cpp')))
            rows.append(dict(rows[-1],file=str(mesa/'src/c11/impl/threads_win32_tls_callback.cpp'),output='threads_win32_tls_callback'))
    for name,h in ref['mesa_pco_sources'].items():
        if sha(mesa/name)!=h:raise SystemExit('PCO source mismatch: '+name)
    reuse={}
    if a.reuse_directory:
        previous=json.loads((a.reuse_directory/'build-manifest.json').read_text())
        if previous['reference_manifest_sha256']!=sha(a.reference_manifest):raise SystemExit('reuse reference mismatch')
        reuse={r['source']:r for r in previous['units']}
    out.mkdir(parents=True,exist_ok=False)
    if a.component=='pco':
        rows.extend(dict(rows[0],file=str(here/(unit+'.c')),output=unit+'_adapter') for unit in ('pco_sm2','sm2'))
    platform_vfs=a.msvc_vfs
    if a.msvc_sdk:
        # Clang ARM supports real GCC atomics with acquire/release ordering.
        # Mesa's stock MSVC path lacks ARM32 lower-case 64-bit helpers. Keep
        # the source checkout intact and select the Clang path via a VFS copy.
        original=mesa/'src/util/u_atomic.h';text=original.read_text()
        old='#elif defined(_MSC_VER)\n#define PIPE_ATOMIC_MSVC_INTRINSIC\n#elif defined(__GNUC__)'
        new='#elif defined(_MSC_VER) && !(defined(__clang__) && defined(_M_ARM))\n#define PIPE_ATOMIC_MSVC_INTRINSIC\n#elif defined(__GNUC__) || (defined(__clang__) && defined(_M_ARM))'
        if text.count(old)!=1:raise SystemExit('Unreviewed Mesa atomic selector')
        overlay=out/'u_atomic_arm32.h';overlay.write_text(text.replace(old,new))
        vfs=json.loads(a.msvc_vfs.read_text()) if a.msvc_vfs else {'version':0,'case-sensitive':'false','roots':[]}
        vfs['roots'].append({'type':'file','name':str(original),'external-contents':str(overlay)})
        debug_source=mesa/'src/imagination/pco/pco_debug.c';debug=debug_source.read_text()
        needle='#include <unistd.h>'
        if debug.count(needle)!=1:raise SystemExit('Unreviewed PCO POSIX debug include')
        debug=debug.replace(needle,'#ifdef _WIN32\n#include <io.h>\n#define isatty _isatty\n#define fileno _fileno\n#else\n#include <unistd.h>\n#endif')
        debug_copy=out/'pco_debug_windows.c';debug_copy.write_text(debug)
        vfs['roots'].append({'type':'file','name':str(debug_source),'external-contents':str(debug_copy)})
        # Microsoft's arm_neon.h describes compiler-specific pseudo intrinsics.
        # Remap only that header to the real Clang intrinsics, keeping the
        # Microsoft intrin.h/CRT headers and user-mode hard-float ABI intact.
        resource=Path(subprocess.check_output([shutil.which('clang'),'-print-resource-dir'],text=True).strip())
        def remap_neon(entry):
            if entry.get('type')=='file' and Path(entry['name']).name.lower()=='arm_neon.h':
                entry['external-contents']=str(resource/'include/arm_neon.h')
            for child in entry.get('contents',[]):remap_neon(child)
        for entry in vfs['roots']:remap_neon(entry)
        vfs['roots'].append({'type':'file','name':str(a.windows_headers.resolve()/'arm_neon.h'),
                             'external-contents':str(resource/'include/arm_neon.h')})
        platform_vfs=out/'platform-vfs.json';platform_vfs.write_text(json.dumps(vfs)+'\n')
    env=os.environ.copy();env.update(TMPDIR=str(out),CCACHE_DISABLE='1',XDG_CACHE_HOME=str(out/'cache'))
    clang=shutil.which('clang');ar=shutil.which('llvm-ar') or shutil.which('llvm-ar-18')
    if not clang or not ar:raise SystemExit('clang/llvm-ar required')
    # Retain only source/generated include paths. NEVER import the host Linux
    # platform defines, pthread flags, x86 ISA, ELF ABI or host dependency stubs.
    def compile_one(row):
        old=shlex.split(row['command']);includes=[]
        for arg in old:
            if arg.startswith('-I'):
                p=(build/arg[2:]).resolve()
                if p.is_relative_to(mesa) or p.is_relative_to(build):includes+=['-I',str(p)]
        obj=out/(Path(row['output']).name+'.obj')
        source=(build/row['file']).resolve();cpp=source.suffix=='.cpp'
        actual_source=source
        if cpp and source.name=='pco_usclib.cpp':
            cpp=False
            text=source.read_text()
            prefix,tail=text.rsplit('namespace {',1)
            match=re.fullmatch(r'\s*struct vtn_bindgen_dummy \{\s*vtn_bindgen_dummy\(\) \{(.*?)\n      \}\s*~vtn_bindgen_dummy\(\) \{(.*?)\n      \}\s*\};\s*static vtn_bindgen_dummy vtn_bindgen_dummy_instance;\s*\}\s*',tail,re.S)
            if not match:raise SystemExit('Unreviewed generated usclib constructor shape')
            actual_source=out/'pco_usclib_explicit.c'
            actual_source.write_text(prefix+'\nvoid geo_pco_usclib_initialize(void) {'+match[1]+'\n}\nvoid geo_pco_usclib_finalize(void) {'+match[2]+'\n}\n')
        platform=['-isystem',str(a.windows_headers.resolve())]
        if a.msvc_sdk and a.component=='util':
            # MSVC exports the underscored CRT names; preserve actual CRT calls.
            platform+=['-Dstrdup=_strdup','-Dstricmp=_stricmp']
        # C-only compiler IR uses GNU packed bitfields (asserted by Mesa).
        # Public C calls keep the Windows ARM ABI. C++ lifetime helpers use
        # the Microsoft ABI and only exchange plain C function arguments.
        target='thumbv7-pc-windows-msvc' if a.msvc_sdk and cpp else 'armv7-w64-windows-gnu'
        if a.msvc_sdk:
            platform+=['-fms-extensions','-fms-compatibility-version=19.0','-D_MSC_VER=1900','-D_M_ARM=7','-D_ARM_','-DARM','-D_ARM_WINAPI_PARTITION_DESKTOP_SDK_AVAILABLE=1','-DUSE_GCC_ATOMIC_BUILTINS','-D_CRT_SECURE_NO_WARNINGS','-DWIN32_LEAN_AND_MEAN','-D_MSC_EXTENSIONS=1','-D_MT','-U__STDC__','-D__STDC__=0','-isystem',str(a.msvc_sdk/'ucrt'),'-isystem',str(a.msvc_sdk/'shared'),'-isystem',str(a.msvc_sdk/'um')]
            if platform_vfs:platform+=['-ivfsoverlay',str(platform_vfs)]
            if not cpp:
                # Preserve UCRT inline bodies under GNU C inline semantics.
                # crt_stdio.cpp emits one real out-of-line copy for varargs
                # and address-taken functions; CRT option state stays shared.
                platform+=['-Dstatic_assert=_Static_assert',
                           '-D_CRT_STDIO_INLINE=extern __inline __attribute__((gnu_inline))']
        cmd=[clang,'--target='+target,'-x','c++' if cpp else 'c','-std=c++14' if cpp else '-std=c11','-O2','-mno-ms-bitfields','-ffreestanding','-fno-builtin','-fno-stack-protector','-ffunction-sections','-fdata-sections','-fno-vectorize','-fno-slp-vectorize','-mno-unaligned-access',
             '-D_WIN32_WINNT=0x0603','-DNTDDI_VERSION=0x06030000','-DGEO_PCO_EXPLICIT_USCLIB_LIFETIME=1','-DHAVE_STRUCT_TIMESPEC','-DHAVE_FUNC_ATTRIBUTE_PACKED','-D_USE_MATH_DEFINES','-DPACKAGE_VERSION="27.0.0-devel"',
             *platform,*includes,'-c',str(actual_source),'-o',str(obj)]
        source_hash=sha((build/row['file']).resolve());old=reuse.get(row['file'])
        if old and old.get('dependencies') and old['exit_code']==0 and old['source_sha256']==source_hash and old['command'][:-1]==cmd[:-1] and all(sha(Path(n))==h for n,h in old['dependencies'].items()):
            old_object=Path(old['object'])
            if not old.get('object_sha256') or sha(old_object)!=old['object_sha256']:raise SystemExit('reuse object hash mismatch')
            shutil.copyfile(old_object,obj)
            return dict(old,object=str(obj),command=cmd,reused=True)
        with (out/(obj.name+'.log')).open('w') as log:
            result=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT)
        dependencies={}
        if result.returncode==0:
            deptext=subprocess.check_output(cmd[:-2]+['-M'],env=env,text=True)
            dependencies={n:sha(Path(n)) for n in shlex.split(deptext.replace('\\\n',' '))[1:]}
        return {'source':row['file'],'source_sha256':sha(source),'compiled_source_sha256':sha(actual_source),'command':cmd,'object':str(obj),'exit_code':result.returncode,'object_sha256':sha(obj) if result.returncode==0 else None,'dependencies':dependencies}
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(compile_one,rows))
    report={'reference_manifest_sha256':sha(a.reference_manifest),'component':a.component,'target':'Windows ARM C ABI / GNU packed private IR / MSVC C++ lifetime' if a.msvc_sdk else 'armv7-w64-windows-gnu','units':results,
            'host_platform_flags_reused':False,'runtime_verified':False,'msvc_wdk_linked':False,'nir_util_dependencies_linked':False,'builder_sha256':sha(Path(__file__)),
            'compiler_version':subprocess.check_output([clang,'--version'],text=True),'private_bitfield_abi':'-mno-ms-bitfields; size assertions retained; does not apply to Windows DDI objects'}
    failed=[r for r in results if r['exit_code']]
    if not failed:
        lib=out/('ge8300-'+a.component+'-arm32.lib');subprocess.run([ar,'rcs',str(lib),*[r['object'] for r in results]],check=True,env=env)
        report['archive_sha256']=sha(lib)
    (out/'build-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    if failed:raise SystemExit('ARM compiler dependency failed: '+', '.join(x['source'] for x in failed))
    print('ARM32 '+a.component+' archive built; complete dependency closure and final WDK DLL link remain required')
if __name__=='__main__':main()

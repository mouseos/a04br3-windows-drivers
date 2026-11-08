#!/usr/bin/env python3
"""Real thumbv7 Windows ARM COFF compile/archive; exact object retention."""
import argparse,copy,importlib.util,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);repo=out/'source';repo.mkdir()
def module(name,file):
 s=importlib.util.spec_from_file_location(name,root/'tools/ci'/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
planner=module('planner','plan-ge8300-incremental-runtime.py');builder=module('builder','build-ge8300-runtime-plan.py')
def run(*args):return subprocess.check_output(args,cwd=repo,text=True).strip()
run('git','init','-q');run('git','config','user.name','yuma shimizu');run('git','config','user.email','46545607+mouseos@users.noreply.github.com');assert run('git','config','user.email').endswith('@users.noreply.github.com')
(repo/'shared.h').write_text('#define V 7\n')
for i in range(3):(repo/f'u{i}.c').write_text(('#include "shared.h"\n' if i<2 else '#define V 1\n')+f'int fn{i}(void){{return V;}}\n')
run('git','add','.');run('git','commit','-qm','Pinned three ARM input units');base=run('git','rev-parse','HEAD')
clang=Path(shutil.which('clang')).resolve();ar=Path(subprocess.check_output([str(clang),'-print-prog-name=llvm-ar'],text=True).strip()).resolve()
flags=['--target=thumbv7-pc-windows-msvc','-std=c11','-O2','-ffreestanding','-nostdinc'];units=[{'source':f'u{i}.c','object':f'u{i}.obj','arguments':flags} for i in range(3)]
old=out/'prior';old.mkdir();members=[]
for u in units:
 obj=old/u['object'];subprocess.run([str(clang),*flags,'-c',str(repo/u['source']),'-o',str(obj)],check=True);members.append({'name':u['object'],'sha256':builder.sha(obj)})
archive=old/'prior.lib';subprocess.run([str(ar),'rcsD',str(archive),*[str(old/u['object']) for u in units]],check=True)
prior={'source_commit':base,'clang_sha256':builder.sha(clang),'archive_sha256':builder.sha(archive),'members':members,'units':units};receipt=old/'receipt.json';receipt.write_text(json.dumps(prior));h=builder.sha(receipt)
(repo/'shared.h').write_text('#define V 13\n');run('git','add','.');run('git','commit','-qm','Shared header changes two runtime consumers')
plan=planner.plan(repo,base,units,units,clang,builder.sha(clang),[]);planfile=out/'plan.json';planfile.write_text(json.dumps(plan));planhash=builder.sha(planfile)
m=builder.execute(repo,planfile,planhash,archive,receipt,h,clang,ar,builder.sha(ar),out/'delta-build');assert m['compiled_count']==2 and m['retained_count']==1
assert (out/'delta-build/objects/u2.obj').read_bytes()==(old/'u2.obj').read_bytes();cases=['actual ARM COFF changed header compiles2 and retains1 exact object']
prior2={'source_commit':m['source_commit'],'clang_sha256':m['clang_sha256'],'archive_sha256':m['archive_sha256'],'members':m['members'],'units':m['units']};receipt2=out/'prior2.json';receipt2.write_text(json.dumps(prior2))
plan2=planner.plan(repo,m['source_commit'],units,units,clang,builder.sha(clang),[]);plan2file=out/'plan2.json';plan2file.write_text(json.dumps(plan2));m2=builder.execute(repo,plan2file,builder.sha(plan2file),out/'delta-build/runtime-delta.lib',receipt2,builder.sha(receipt2),clang,ar,builder.sha(ar),out/'retained-build');assert m2['compiled_count']==0 and m2['retained_count']==3;cases.append('all unchanged ARM objects reused without compile')
# Keep malformed inputs isolated; baseline production inputs stay intact.
bad=out/'bad.lib';bad.write_bytes(archive.read_bytes()+b'corruption')
try:builder.execute(repo,planfile,planhash,bad,receipt,h,clang,ar,builder.sha(ar),out/'bad-archive');raise AssertionError('Expected archive pin rejection')
except ValueError as e:assert 'Pinned input mismatch' in str(e)
cases.append('corrupted baseline archive rejected')
(repo/'shared.h').write_text('#define V 17\n');run('git','add','.');run('git','commit','-qm','Source changed after plan generation')
try:builder.execute(repo,planfile,planhash,archive,receipt,h,clang,ar,builder.sha(ar),out/'stale-plan');raise AssertionError('Expected stale HEAD rejection')
except ValueError as e:assert 'HEAD changed' in str(e)
cases.append('changed source HEAD after planning rejected')
result={'schema':'real-ARM-runtime-plan-build-tests-v1','cases_passed':len(cases),'cases':cases,'real_ARM_COFF_compiled':True,'archive_built':True,'signed':False,'hardware_operations':False};(out/'receipt.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

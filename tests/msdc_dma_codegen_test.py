#!/usr/bin/env python3
"""Compare O0/O2 unchanged DMA code including every I/O and clock callback."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess

p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[1];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,TMPDIR=str(out),TMP=str(out),TEMP=str(out))
d=r/'drivers/storage/mt8167-msdc';cc=os.environ.get('CC','clang')
common=[cc,'-std=c11','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(d)]
def run(args):return subprocess.run(args,env=env,check=True,capture_output=True,text=True)
for level in (0,2):
    renames=[f'-Da04_msdc_dma_sector_timed=dma_o{level}',f'-Da04_msdc_dma_read_sector=read_o{level}',f'-Da04_msdc_dma_write_sector=write_o{level}']
    run(common+[f'-O{level}','-ffreestanding','-fno-vectorize','-fno-slp-vectorize',*renames,'-c',str(d/'dma.c'),'-o',str(out/f'dma-o{level}.o')])
run(common+['-O1',str(r/'tests/msdc_dma_codegen_test.c'),str(out/'dma-o0.o'),str(out/'dma-o2.o'),'-o',str(out/'compare')])
result=json.loads(run([str(out/'compare')]).stdout)
result.update(dma_source_sha256=hashlib.sha256((d/'dma.c').read_bytes()).hexdigest(),compiler=run([cc,'--version']).stdout.splitlines()[0],ASan_UBSan=True)
(out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))

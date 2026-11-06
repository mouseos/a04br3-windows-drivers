#!/usr/bin/env python3
"""Exercise the actual ISR-synchronized permanent source mask."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];g=root/'drivers/gpu/mt8167'
s=(g/'vendor_m1_11/windows_irq.c').read_text()
def fn(name):
 a=s.index(name);b=s.index('{',a);e=b+1;n=1
 while n:n+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r'''
#include <assert.h>
#include <stdint.h>
#include "fwif/pvr_rogue_cr_defs.h"
typedef unsigned ULONG;typedef int LONG;typedef int BOOLEAN;typedef unsigned char UCHAR;
typedef void *WDFINTERRUPT;typedef void *WDFCONTEXT;
#define TRUE 1
#define FALSE 0
#define UNREFERENCED_PARAMETER(x) (void)(x)
struct a04_vendor_irq {void *interrupt;UCHAR *core;LONG powered,masked;};
static int locked,fail,writes;
static LONG InterlockedCompareExchange(LONG*p,int v,int cmp){int old=*p;if(old==cmp)*p=v;return old;}
static LONG InterlockedExchange(LONG*p,int v){int old=*p;*p=v;return old;}
static void WRITE_REGISTER_ULONG(volatile ULONG*p,ULONG v){assert(locked);++writes;if(!fail)*p=v;}
static ULONG READ_REGISTER_ULONG(volatile ULONG*p){assert(locked);return *p;}
static BOOLEAN WdfInterruptSynchronize(void*i,BOOLEAN(*f)(void*,void*),void*c){assert(!locked);locked=1;int ok=f(i,c);locked=0;return ok;}
'''+fn('static BOOLEAN a04_vendor_retire_synchronized(')+'\n'+fn('BOOLEAN a04_vendor_irq_retire(')+r'''
int main(void){
 ULONG bank[4096]={0};struct a04_vendor_irq q={&q,(UCHAR*)bank,1,0};
 bank[ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE/4]=1;
 assert(a04_vendor_irq_retire(&q) && !q.powered && q.masked && writes==1);
 assert(!a04_vendor_irq_retire(&q) && writes==1); /* No MMIO after closure. */
 q.powered=1;q.masked=0;fail=1;bank[ROGUE_CR_MIPS_WRAPPER_IRQ_ENABLE/4]=1;
 assert(!a04_vendor_irq_retire(&q) && q.powered && !q.masked);
 assert(!a04_vendor_irq_retire(0));return 0;
}
'''
with tempfile.TemporaryDirectory(dir=os.environ['TMPDIR']) as tmp:
 tmp=Path(tmp);p=tmp/'test.c';p.write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(g),str(p),'-o',str(tmp/'test')],check=True)
 subprocess.run([str(tmp/'test')],check=True)
print('actual IRQ retirement: ISR serialization, mask readback failure and no late MMIO passed')

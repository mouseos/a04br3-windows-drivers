#!/usr/bin/env python3
"""Exercise the real IRQ observation and drained checks with mock atomics."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'drivers/gpu/mt8167/vendor_m1_11/windows_irq.c').read_text()
def function(name):
 a=s.index('BOOLEAN '+name+'(');b=s.index('{',a);i=b+1;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
pre='''#include <assert.h>
#include <stddef.h>
typedef unsigned ULONG; typedef int BOOLEAN; typedef int LONG;
#define TRUE 1
#define FALSE 0
struct a04_vendor_irq { void *core; LONG powered,sequence,serviced,work_busy; };
static LONG InterlockedCompareExchange(LONG *p,LONG n,LONG e){LONG x=*p;if(x==e)*p=n;return x;}
'''
main='''
int main(void){struct a04_vendor_irq q={0};ULONG out[4]={9,9,9,9};
 assert(!a04_vendor_irq_observe(NULL,out));assert(!a04_vendor_irq_observe(&q,NULL));
 q.core=&q;q.powered=1;q.sequence=3;q.serviced=2;q.work_busy=1;
 assert(a04_vendor_irq_observe(&q,out));assert(out[0]==1&&out[1]==3&&out[2]==2&&out[3]==1);
 assert(!a04_vendor_irq_work_drained(&q));q.work_busy=0;assert(!a04_vendor_irq_work_drained(&q));
 q.serviced=3;assert(a04_vendor_irq_work_drained(&q));q.powered=0;assert(!a04_vendor_irq_work_drained(&q));
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';x=Path(d)/'test';p.write_text(pre+function('a04_vendor_irq_work_drained')+function('a04_vendor_irq_observe')+main)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(x)],check=True)
 subprocess.run([str(x)],check=True)
print('IRQ observation and drained predicate: PASS')

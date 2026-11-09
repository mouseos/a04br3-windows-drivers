"""実sourceのCM判定をhost compileし、失敗/不在/型の境界を検証する。実機I/O無し。"""
import argparse,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(r.parent.parent.resolve()) or out.exists():raise ValueError('root内新規検証dirが必要')
out.mkdir()
s=(r/'tools/winpe/engine-deferred-update.h').read_text();fn=s[s.index('static DWORD deferred_observe('):s.index('static DWORD deferred_find(')]
h='''#include <stdint.h>
#include <assert.h>
typedef uint32_t DWORD,ULONG,CONFIGRET,DEVPROPTYPE,DEVINST;typedef unsigned char BYTE;
#define CR_SUCCESS 0
#define CR_NO_SUCH_VALUE 37
#define DN_STARTED 8
#define DN_HAS_PROBLEM 1024
#define ERROR_NOT_READY 21
#define DEVPROP_TYPE_NTSTATUS 25
#define EV_PNP_STATUS_CR 0
#define EV_PNP_NT_TYPE 0
#define EV_PNP_NT_BYTES 0
static DWORD scr,flags,problem,pcr,ptype,pbytes,ntvalue,snapshot_cr;
static int problem_status_key;
static CONFIGRET status(ULONG *s,ULONG *p,DEVINST n,ULONG f){*s=flags;*p=problem;return scr;}
static CONFIGRET prop(DEVINST n,void*k,DEVPROPTYPE*t,BYTE*b,ULONG*z,ULONG f){*t=ptype;*z=pbytes;*(DWORD*)b=ntvalue;return pcr;}
struct pnp_apis{CONFIGRET(*status)(ULONG*,ULONG*,DEVINST,ULONG);CONFIGRET(*devproperty)(DEVINST,void*,DEVPROPTYPE*,BYTE*,ULONG*,ULONG);};
static void pnp_record(unsigned i,DWORD e,DWORD v){}
static void pnp_bind_metadata(struct pnp_apis*a,unsigned i,DEVINST n,int k,char*l){}
static void diagnostic_node(unsigned i,CONFIGRET s,DWORD f,DWORD p,CONFIGRET c,DEVPROPTYPE t,ULONG b,DWORD n){snapshot_cr=c;}
'''
t='''int main(void){struct pnp_apis a={status,prop};scr=0;flags=DN_STARTED;problem=0;pcr=37;
assert(deferred_observe(&a,0,0)==0&&snapshot_cr==37);
flags=0;assert(deferred_observe(&a,0,0)==21);flags=DN_STARTED|DN_HAS_PROBLEM;assert(deferred_observe(&a,0,0)==21);
flags=DN_STARTED;problem=1;assert(deferred_observe(&a,0,0)==21);problem=0;scr=1;assert(deferred_observe(&a,0,0)==21);scr=0;
pcr=38;assert(deferred_observe(&a,0,0)==21);pcr=0;ptype=25;pbytes=4;ntvalue=0;assert(deferred_observe(&a,0,0)==0);
ntvalue=0xc0000001;assert(deferred_observe(&a,0,0)==21);ntvalue=0;pbytes=3;assert(deferred_observe(&a,0,0)==21);pbytes=4;ptype=7;assert(deferred_observe(&a,0,0)==21);return 0;}
'''
c=out/'cm-contract.c';c.write_text(h+fn+t)
subprocess.run(['cc','-std=c99',str(c),'-o',str(out/'cm-contract')],check=True);subprocess.run([str(out/'cm-contract')],check=True)
print('actual deferred_observe source: CR37 healthy-only / query error / malformed type / NT failure boundaries PASS')

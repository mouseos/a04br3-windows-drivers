#!/usr/bin/env python3
"""Execute the actual four-CPU poll with simulated WDF, CPU and MMIO."""
from pathlib import Path
import os, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
src = (root/'drivers/system/mt8167-toprgu/wdf_toprgu.c').read_text()
start = src.index('static VOID ToprguPoll(TOPRGU_CONTEXT *c, BOOLEAN backup)')
end = src.index('\n#else\nNTSTATUS ToprguD0Entry', start)
poll = src[start:end]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "handoff_policy.h"
#include "watchdog_policy.h"
#define A04_TOPRGU_THERMAL_STOP 0
#define A04_TOPRGU_DEADLINE_SECONDS 0
#define VOID void
#define TRUE 1
#define FALSE 0
typedef unsigned long ULONG; typedef unsigned long long ULONGLONG; typedef int BOOLEAN; typedef unsigned char UCHAR;
typedef union { long long QuadPart; } LARGE_INTEGER;
typedef struct { volatile ULONG *wdt,*uart; long running,busy,backupStarted,ownedFour; int backupDpc,backupTimer,suspendedWatchdog,deadlineReported; ULONG savedMode,ticks,backupCalls; ULONGLONG started; int parser; } TOPRGU_CONTEXT;
static unsigned active=4,cpu=1,affinity=1,writes,queries,drop_at;
static long InterlockedCompareExchange(volatile long *p,long n,long old) {long v=*p;if(v==old)*p=n;return v;}
static long InterlockedExchange(volatile long *p,long n) {long v=*p;*p=n;return v;}
static unsigned KeQueryActiveProcessorCount(void *p) {(void)p;if (++queries==drop_at) active=3;return active;}
static unsigned KeGetCurrentProcessorNumber(void) {return cpu;}
static unsigned ToprguMpidrAffinity(void) {return affinity;}
static unsigned long long KeQueryInterruptTime(void) {return 100000;}
static void KeSetTargetProcessorDpc(int *p,int n) {(void)p;(void)n;}
static void KeSetTimerEx(int *p,LARGE_INTEGER d,int n,int *q) {(void)p;(void)d;(void)n;(void)q;}
static ULONG readreg(volatile ULONG *p) {return *p;}
static void writereg(volatile ULONG *p,ULONG v) {*p=v;writes++;}
#define READ_REGISTER_ULONG readreg
#define WRITE_REGISTER_ULONG writereg
static void Trace(TOPRGU_CONTEXT *c,const char *s) {(void)c;(void)s;}
static void TraceTimer(TOPRGU_CONTEXT *c,ULONG a,ULONGLONG b) {(void)c;(void)a;(void)b;}
static int mt8167_reset_consume(int *p,UCHAR ch) {(void)p;(void)ch;return 0;}
'''
main = r'''
int main(void) {
 ULONG w[16]={1},u[16]={0}; TOPRGU_CONTEXT c={0}; c.wdt=w;c.uart=u;c.running=1;
 ToprguPoll(&c,0); assert(!c.ownedFour && writes==0);
 ToprguPoll(&c,1); assert(c.ownedFour && writes==1);
 active=3; ToprguPoll(&c,0); assert(!c.ownedFour && writes==1);
 active=4; cpu=0; ToprguPoll(&c,0); assert(!c.ownedFour && writes==1);
 cpu=1; affinity=2; ToprguPoll(&c,1); assert(!c.ownedFour && writes==1);
 affinity=1; ToprguPoll(&c,1); assert(c.ownedFour && writes==2);
 active=3; ToprguPoll(&c,0); active=4; w[0]=0;
 ToprguPoll(&c,1); assert(!c.ownedFour && writes==2);
 w[0]=1; active=4; queries=0; drop_at=3;
 ToprguPoll(&c,1); assert(!c.ownedFour && writes==2);
 {
   ULONG rw[16]={0},ru[16]={0}; TOPRGU_CONTEXT resumed={0};
   resumed.wdt=rw; resumed.uart=ru; resumed.running=1;
   resumed.suspendedWatchdog=1; resumed.savedMode=1;
   active=4; queries=drop_at=0; cpu=0;
   ToprguPoll(&resumed,0); assert(resumed.suspendedWatchdog && writes==2);
   cpu=1; ToprguPoll(&resumed,1);
   assert(!resumed.suspendedWatchdog && resumed.ownedFour && (rw[0]&1) && writes==6);
 }
 puts("actual poll: claim, CPU loss, re-claim and disabled MODE: OK"); return 0;
}
'''
tmp = root/'build'; tmp.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='toprgu-host-', dir=tmp) as d:
    path=Path(d)/'poll.c'; path.write_text(harness+poll+main)
    exe=Path(d)/'poll'
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'drivers/system/mt8167-toprgu'),str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

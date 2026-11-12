#!/usr/bin/env python3
"""実Trace/expiry関数をmock MMIO/clockへ結合。端末・Windowsを操作しない。"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'drivers/system/mt8167-toprgu/wdf_toprgu.c').read_text()


def function(name):
    start = source.index('static '+name)
    begin = source.index('{', start)
    depth = 1
    end = begin+1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "winpe_lease.h"
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
typedef unsigned char UCHAR;
typedef int BOOLEAN;
typedef struct {
    volatile ULONG *wdt, *uart;
    ULONG screenLen;
    struct a04_winpe_lease lease;
} TOPRGU_CONTEXT;
static ULONG wdt[8], uart[8];
static uint64_t now, reset_at, ready_at;
static unsigned reads, stalls, resets, sent, clock_advances, uart_mode;
static char message[128];
static ULONGLONG KeQueryInterruptTime(void) { return now; }
static void KeStallExecutionProcessor(ULONG us) {
    assert(us==1); stalls++;
    now+=10u*clock_advances;
}
static ULONG readreg(volatile ULONG *p) {
    assert(!resets); /* No UART operation is allowed after requesting reset. */
    if (p==uart+3) return uart_mode==3 ? 0x80u : 0;
    assert(p==uart+5); reads++;
    if (uart_mode==1 || uart_mode==3) return 0;
    return 0x20u | (now>=ready_at ? 0x40u : 0);
}
static void writereg(volatile ULONG *p, ULONG value) {
    if (p==wdt+5) {
        assert(value==0x1209u && !resets);
        resets++; reset_at=now;
    } else {
        assert(p==uart && !resets && sent+1<sizeof(message));
        message[sent++]=(char)value; message[sent]=0;
    }
}
#define READ_REGISTER_ULONG readreg
#define WRITE_REGISTER_ULONG writereg
'''
main = r'''
static void run(unsigned mode, int advancing, uint64_t ready, unsigned pending) {
    const uint64_t span=UINT64_C(1800)*10000000u;
    TOPRGU_CONTEXT c={0};
    memset(wdt,0,sizeof(wdt)); memset(uart,0,sizeof(uart));
    reads=stalls=resets=sent=0; message[0]=0;
    now=span; reset_at=0; ready_at=span+ready;
    uart_mode=mode; clock_advances=advancing;
    c.wdt=wdt; c.uart=mode==4 ? NULL : uart; c.screenLen=pending;
    a04_winpe_lease_start(&c.lease,0);
    now=span-1;
    assert(!ToprguWinpeLeaseExpired(&c) && !reads && !resets && !sent);
    now=span;
    assert(ToprguWinpeLeaseExpired(&c) && resets==1 && c.lease.expired);
    assert(stalls<=20000);
    if ((mode==0 || mode==5) && !pending) {
        assert(!strcmp(message,"A04RGU WinPE lease expired; SWRST requested\n"));
        assert(reset_at>=ready_at || stalls==20000 || reset_at-span>=200000);
    }
    if (mode==1 || mode==3) assert(!sent);
    if (pending || mode==4) assert(!sent);
    if (mode==1 || (mode==0 && ready>200000)) assert(stalls==20000);
    if (mode==4) assert(!reads && !stalls);
    if (mode==5) assert(stalls==20 && reset_at==span+200000);
    /* Failed SWRST must never retry the log/reset or renew the lease. */
    {
        unsigned old_reads=reads, old_sent=sent, old_stalls=stalls;
        a04_winpe_lease_start(&c.lease,now+span);
        now+=span;
        assert(ToprguWinpeLeaseExpired(&c));
        now=0; assert(ToprguWinpeLeaseExpired(&c));
        assert(resets==1 && reads==old_reads && sent==old_sent && stalls==old_stalls);
    }
}
int main(void) {
    run(0,1,0,0);       /* TEMT already ready. */
    run(0,1,50000,0);   /* THRE ready, shift register busy for 5ms. */
    run(0,1,300000,0);  /* TEMT stays low past the limit. */
    run(0,0,300000,0);  /* THRE works but clock/TEMT are stuck. */
    run(5,1000,300000,0); /* Clock limit ends drain before iteration limit. */
    run(1,1,0,0);       /* Both TX status bits remain low. */
    run(1,0,0,0);       /* UART and clock stuck: iteration limit still resets. */
    run(3,0,0,0);       /* DLAB set: Trace cannot send; drain still bounded. */
    run(4,0,0,0);       /* Missing UART never blocks reset. */
    run(0,1,50000,1);   /* Preserve existing screen-packet Trace suppression. */
    puts("actual Trace/expiry: 10 MMIO/clock scenarios, ordered SWRST, bounded drain, one-reset/no-renew: OK");
    return 0;
}
'''
build = root/'build'
build.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='toprgu-expiry-host-', dir=build) as d:
    c = Path(d)/'expiry.c'
    c.write_text(harness+function('void Trace(')+'\n'+
                 function('BOOLEAN ToprguWinpeLeaseExpired(')+main)
    exe = Path(d)/'expiry'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-DA04_TOPRGU_WINPE_LEASE_SECONDS=1800', '-DA04_TOPRGU_CPU1_HANDOFF=1',
                    '-DA04_TOPRGU_UART_SCREEN=1', '-I', str(root/'drivers/system/mt8167-toprgu'),
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

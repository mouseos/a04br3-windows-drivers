/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "ge8300_trace.hxx"
#ifndef A04_GE8300_WDDM_COPY
#define A04_GE8300_WDDM_COPY 0
#endif
#if A04_GE8300_WDDM_COPY
static void WriteText(volatile ULONG* uart,const char* text)
{
    while (*text) {
        ULONG tries;
        for (tries=100000;tries;--tries)
            if (!(READ_REGISTER_ULONG(uart+3)&0x80u) && (READ_REGISTER_ULONG(uart+5)&0x20u)) break;
        if (!tries) return;
        WRITE_REGISTER_ULONG(uart,(ULONG)(UCHAR)*text++);
    }
}
static void WriteHex(volatile ULONG* uart,ULONG value)
{
    static const char digits[]="0123456789ABCDEF"; char data[9];
    for (unsigned i=0;i<8;++i) data[i]=digits[(value>>(28-4*i))&15u];
    data[8]=0; WriteText(uart,data);
}
#endif
void A04DisplayTrace(ULONG stage,NTSTATUS status)
{
#if A04_GE8300_WDDM_COPY
    PHYSICAL_ADDRESS base; base.QuadPart=0x11005000u;
    volatile ULONG* uart=(volatile ULONG*)MmMapIoSpace(base,0x1000,MmNonCached);
    if (!uart) return;
    WriteText(uart,"A04DISP STEP="); WriteHex(uart,stage);
    WriteText(uart," STATUS="); WriteHex(uart,(ULONG)status); WriteText(uart,"\n");
    MmUnmapIoSpace((PVOID)uart,0x1000);
#else
    UNREFERENCED_PARAMETER(stage); UNREFERENCED_PARAMETER(status);
#endif
}

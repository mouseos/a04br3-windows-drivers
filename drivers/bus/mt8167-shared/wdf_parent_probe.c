/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental PnP parent. D0 rejects by default; the opt-in button D0
 * experiment checks WACS2 readiness and admits the fixed button transaction.
 * Never deploy while existing button/touch drivers claim the same resources. */
#include <initguid.h>
#include <ntddk.h>
#include <wdf.h>
#include "parent_resources.h"
#include "pwrap_owner.h"
#include "register_owner.h"
#include "client_interface.h"
#include "audio_interface.h"
#include "audio_owner.h"
#include "usb_interface.h"
#include "usb_owner.h"
#ifndef A04_SHARED_THERMAL_DIAGNOSTIC
#define A04_SHARED_THERMAL_DIAGNOSTIC 0
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
#include "thermal_interface.h"
#include "thermal_owner.h"
#endif
#ifndef A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
#define A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC 0
#endif
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC && (!defined(A04_SHARED_MT6392_DIAGNOSTIC) || !A04_SHARED_MT6392_DIAGNOSTIC || !defined(A04_SHARED_APMIXED_RESOURCE) || !A04_SHARED_APMIXED_RESOURCE)
#error codec domain requires MT6392 diagnostics and APMIXED resources
#endif
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
#include "codec_domain.h"
#endif
#ifndef A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC
#define A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC 0
#endif
#if A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC && (!defined(A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC) || !A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC)
#error IRQ prep requires AFE clock diagnostic
#endif
#ifndef A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
#define A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC 0
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC && (!defined(A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC) || !A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC)
#error AFE gate diagnostic requires TOP audio gate diagnostic
#endif
#ifndef A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
#define A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC 0
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC && (!defined(A04_SHARED_QUERY_EXPERIMENTAL) || !A04_SHARED_QUERY_EXPERIMENTAL || !defined(A04_SHARED_BUTTON_D0_EXPERIMENT) || !A04_SHARED_BUTTON_D0_EXPERIMENT)
#error TOP audio gate diagnostic requires shared query and D0
#endif
#ifndef A04_AUDIO_DIAGNOSTIC_TRACE
#define A04_AUDIO_DIAGNOSTIC_TRACE 0
#endif
#ifndef A04_AUDIO_PMIC_TRACE
#define A04_AUDIO_PMIC_TRACE 0
#endif
#if A04_AUDIO_PMIC_TRACE && !A04_AUDIO_DIAGNOSTIC_TRACE
#error PMIC observation requires diagnostic trace
#endif
#if A04_AUDIO_DIAGNOSTIC_TRACE
#define A04_TRACE(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,__VA_ARGS__)
#else
#define A04_TRACE(...) ((void)0)
#endif
#ifndef A04_MT6392_CLK_ALIAS_EXPERIMENT
#define A04_MT6392_CLK_ALIAS_EXPERIMENT 0
#endif
#ifndef A04_SHARED_MT6392_DIAGNOSTIC
#define A04_SHARED_MT6392_DIAGNOSTIC 0
#endif
#if A04_SHARED_MT6392_DIAGNOSTIC
#include "mt6392_classd.h"
#if !defined(A04_MT6392_WRITE_DIAGNOSTIC) || !A04_MT6392_WRITE_DIAGNOSTIC
#error MT6392 diagnostic requires the guarded WACS2 writer
#endif
#endif
#ifndef A04_SHARED_APMIXED_RESOURCE
#define A04_SHARED_APMIXED_RESOURCE 0
#endif
#if A04_SHARED_APMIXED_RESOURCE != 0 && A04_SHARED_APMIXED_RESOURCE != 1
#error A04_SHARED_APMIXED_RESOURCE must be 0 or 1
#endif
#ifndef A04_SHARED_BUTTON_D0_EXPERIMENT
#define A04_SHARED_BUTTON_D0_EXPERIMENT 0
#endif
#ifndef A04_SHARED_TOUCH_D0_EXPERIMENT
#define A04_SHARED_TOUCH_D0_EXPERIMENT 0
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC && (!defined(A04_SHARED_QUERY_EXPERIMENTAL) || !A04_SHARED_QUERY_EXPERIMENTAL || !defined(A04_SHARED_CHILD_PROBE) || !A04_SHARED_CHILD_PROBE || !A04_SHARED_BUTTON_D0_EXPERIMENT || !A04_SHARED_TOUCH_D0_EXPERIMENT || !A04_SHARED_APMIXED_RESOURCE)
#error Shared thermal diagnostic requires query, children, touch D0 and APMIXED
#endif
#if A04_SHARED_TOUCH_D0_EXPERIMENT && !A04_SHARED_BUTTON_D0_EXPERIMENT
#error Shared touch requires the guarded shared parent D0 experiment
#endif
#if A04_MT6392_CLK_ALIAS_EXPERIMENT && !A04_SHARED_MT6392_DIAGNOSTIC
#error A04_MT6392_CLK_ALIAS_EXPERIMENT requires MT6392 diagnostics
#endif
#if A04_SHARED_MT6392_DIAGNOSTIC && (!defined(A04_SHARED_QUERY_EXPERIMENTAL) || !A04_SHARED_QUERY_EXPERIMENTAL || !A04_SHARED_APMIXED_RESOURCE || !A04_SHARED_BUTTON_D0_EXPERIMENT)
#error A04_SHARED_MT6392_DIAGNOSTIC requires query, APMIXED and button D0 diagnostics
#endif
#if A04_SHARED_BUTTON_D0_EXPERIMENT && (!defined(A04_SHARED_QUERY_EXPERIMENTAL) || !A04_SHARED_QUERY_EXPERIMENTAL || !defined(A04_SHARED_CHILD_PROBE) || !A04_SHARED_CHILD_PROBE)
#error A04_SHARED_BUTTON_D0_EXPERIMENT requires the shared query and child topology
#endif
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
#include "shared_interface.h"
#endif
typedef struct { PVOID topck,infra,gpio,pwrap,apmixed; ULONG apmixedLength; FAST_MUTEX pwrapLock; volatile LONG admissionClosed,removed; struct a04_pwrap_owner pmic; struct a04_register_owner regs; struct a04_audio_owner audio; struct a04_usb_owner usb;
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
struct a04_codec_domain codecDomain;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
struct a04_shared_thermal_owner thermal;EX_RUNDOWN_REF thermalRundown;BOOLEAN thermalRundownClosed;
#endif
PVOID audioAfe;
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
int afeClockHeld,afeClockRestore,afeClockPoisoned;
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
struct a04_audio_top_clock audioTopClock;
#endif
#if A04_SHARED_MT6392_DIAGNOSTIC
struct a04_mt6392_classd classd;
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
struct a04_pmic_spk_clock speakerClock;
#endif
#endif
} A04_PARENT_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_PARENT_CONTEXT,ParentContext)
EVT_WDF_DRIVER_DEVICE_ADD ParentAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ParentPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE ParentRelease;
EVT_WDF_DEVICE_D0_ENTRY ParentD0;
EVT_WDF_DEVICE_D0_EXIT ParentD0Exit;
EVT_WDF_DEVICE_SURPRISE_REMOVAL ParentSurprise;
static int ParentLock(void *context) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || KeGetCurrentIrql()>APC_LEVEL || InterlockedCompareExchange(&c->admissionClosed,0,0))return -1;
    ExAcquireFastMutex(&c->pwrapLock);
    if(InterlockedCompareExchange(&c->admissionClosed,0,0)){ExReleaseFastMutex(&c->pwrapLock);return -1;}
    return 0;
}
static void ParentUnlock(void *context) { ExReleaseFastMutex(&((A04_PARENT_CONTEXT *)context)->pwrapLock); }
static uint32_t ParentPwrapRead(void *context,unsigned offset) {
    A04_PARENT_CONTEXT *c=(A04_PARENT_CONTEXT *)context;
    return READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+offset));
}
static void ParentPwrapWrite(void *context,unsigned offset,uint32_t value) {
    A04_PARENT_CONTEXT *c=(A04_PARENT_CONTEXT *)context;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+offset),value);
}
static void ParentPwrapDelay(void *context) { UNREFERENCED_PARAMETER(context);KeStallExecutionProcessor(1); }
static int ParentRegRead(void *context,enum a04_shared_bank bank,unsigned offset,uint32_t *value)
{
    A04_PARENT_CONTEXT *c=context; PVOID base;
    if (!value || offset>0xffcu || (offset&3)) return -1;
    base=bank==A04_SHARED_TOP?c->topck:bank==A04_SHARED_INFRA?c->infra:bank==A04_SHARED_GPIO?c->gpio:NULL;
    if(!base)return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)base+offset));return 0;
}
static int ParentRegWrite(void *context,enum a04_shared_bank bank,unsigned offset,uint32_t value)
{
    A04_PARENT_CONTEXT *c=context; PVOID base;
    if(offset>0xffcu || (offset&3))return -1;
    base=bank==A04_SHARED_TOP?c->topck:bank==A04_SHARED_INFRA?c->infra:bank==A04_SHARED_GPIO?c->gpio:NULL;
    if(!base)return -1;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)base+offset),value);return 0;
}
static int ParentRegLock(void *context) { return ParentLock(context); }
static void ParentRegUnlock(void *context) { ParentUnlock(context); }
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
static int ParentBank(unsigned bank,enum a04_shared_bank *out)
{
    if(bank==A04_INTERFACE_TOUCH_GPIO)*out=A04_SHARED_GPIO;
    else if(bank==A04_INTERFACE_TOUCH_TOP)*out=A04_SHARED_TOP;
    else if(bank==A04_INTERFACE_TOUCH_INFRA)*out=A04_SHARED_INFRA;
    else return -1;
    return 0;
}
static int ParentTouchUpdate(PVOID context,unsigned bank,unsigned off,ULONG mask,ULONG bits)
{
    enum a04_shared_bank b;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL || ParentBank(bank,&b))return -1;
    return a04_register_touch_update(&ParentContext((WDFDEVICE)context)->regs,b,off,mask,bits);
}
static int ParentTouchWrite(PVOID context,unsigned bank,unsigned off,ULONG value)
{
    enum a04_shared_bank b;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL || ParentBank(bank,&b))return -1;
    return a04_register_touch_write(&ParentContext((WDFDEVICE)context)->regs,b,off,value);
}
static int ParentTouchPen(PVOID context,int *down)
{
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    return a04_register_touch_input(&ParentContext((WDFDEVICE)context)->regs,down);
}
static int ParentButtonRead(PVOID context,USHORT *value)
{
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    return a04_pwrap_owner_read_buttons(&ParentContext((WDFDEVICE)context)->pmic,value);
}
/* Audio owner operations share the PWRAP/register mutex. Platform clock,
 * power and muted-codec callbacks are deliberately unset: no resource set or
 * handoff currently proves their ownership. This path cannot grant a lease. */
#if A04_SHARED_MT6392_DIAGNOSTIC
/* Called under ParentLock by audio_owner; no nested PWRAP lock. The class-D
 * prerequisites and bus callbacks are deliberately unset until a real board
 * mute, retained power, exclusive clock and safe WACS2 writer are proven. */
static int ParentCodecSupports(void *context,uint16_t reg,uint16_t mask,uint16_t value) {
    UNREFERENCED_PARAMETER(context);
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
    if(reg==0x010cu && mask==0x000eu && (value==0 || value==0x000eu))return 0;
#endif
    return a04_pmic_classd_supported(reg,mask,value);
}
static int ParentCodecUpdate(void *context,uint16_t reg,uint16_t mask,uint16_t value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->pmic.online)return -1;
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
    if(reg==0x010cu && mask==0x000eu) {
        if(value==0x000eu)return a04_pmic_spk_clock_acquire(&c->pmic.bus,&c->speakerClock);
        if(value==0)return a04_pmic_spk_clock_release(&c->pmic.bus,&c->speakerClock);
        return -1;
    }
#endif
    return a04_pmic_classd_update(&c->pmic.bus,reg,mask,value);
}
static int ParentCodecRead(void *context,uint16_t reg,uint16_t *value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->pmic.online)return -1;
    return a04_pmic_classd_read_gain(&c->pmic.bus,reg,value);
}
/* Bounded sleeps, not a 20 ms busy stall, while the shared transaction
 * remains serialized. FAST_MUTEX runs the caller at APC_LEVEL. */
static int ParentCodecDelay(void *context,uint32_t us) {
    LARGE_INTEGER interval;NTSTATUS status;
    if(!context || (us!=2000u && us!=20000u) || KeGetCurrentIrql()>APC_LEVEL)return -1;
    interval.QuadPart=-(LONGLONG)us*10;
    status=KeDelayExecutionThread(KernelMode,FALSE,&interval);
    return status==STATUS_SUCCESS ? 0 : -1;
}
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
static int ParentDomainRead(void *context,unsigned offset,uint32_t *value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->apmixed || !value || (offset!=0x700 && offset!=0x704 && offset!=0x708 && offset!=0x70c && offset!=0x710) || offset>c->apmixedLength || c->apmixedLength-offset<sizeof(ULONG))return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+offset));return 0;
}
static int ParentDomainWrite(void *context,unsigned offset,uint32_t value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->apmixed || (offset!=0x700 && offset!=0x704 && offset!=0x708 && offset!=0x70c && offset!=0x710) || offset>c->apmixedLength || c->apmixedLength-offset<sizeof(ULONG))return -1;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+offset),value);return 0;
}
#endif
static int ParentCodecStart(void *context) {
    A04_PARENT_CONTEXT *c=context;struct a04_mt6392_audio_rails rails;
    if(!c || !c->pmic.online || a04_pmic_audio_rails(&c->pmic.bus,&rails))return -1;
    A04_TRACE("A04AUD codec rails vaud22=%04x selector=%04x vaud28=%04x\n",(unsigned)rails.vaud22,(unsigned)rails.vaud22_selector,(unsigned)rails.vaud28);
    if(!a04_pmic_audio_rails_ready(&rails))return -1;
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
    {
        struct a04_codec_domain_io io={c,ParentDomainRead,ParentDomainWrite};
        /* Must prove exclusive shared APMIXED ownership, clocks, retained
         * rails and board silence before even enabling the common domain. */
        if(a04_mt6392_classd_preflight(&c->classd) ||
           a04_codec_domain_start(&c->codecDomain,&io))return -1;
    }
#endif
    return a04_mt6392_classd_start_muted(&c->classd);
}
#if defined(A04_MT6392_UNMUTE_DIAGNOSTIC) && A04_MT6392_UNMUTE_DIAGNOSTIC
static int ParentCodecUnmute(void *context) { return a04_mt6392_classd_unmute_min(&((A04_PARENT_CONTEXT *)context)->classd); }
#endif
static int ParentCodecMute(void *context) { return a04_mt6392_classd_mute(&((A04_PARENT_CONTEXT *)context)->classd); }
static int ParentCodecStop(void *context) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || a04_mt6392_classd_stop(&c->classd))return -1;
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
    {
        struct a04_codec_domain_io io={c,ParentDomainRead,ParentDomainWrite};
        if(a04_codec_domain_stop(&c->codecDomain,&io))return -1;
    }
#endif
    return 0;
}
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
/* These run under ParentLock. No admission is possible until the independent
 * AFE clock, power, codec and quiescence prerequisites are also supplied. */
static int ParentTopAudioAcquire(void *context,uint32_t rate) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->topck || !c->pmic.online || rate!=48000) return -1;
    return a04_audio_top_clock_acquire(&c->regs,&c->audioTopClock);
}
static int ParentTopAudioRelease(void *context) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->topck || !c->pmic.online) return -1;
    return a04_audio_top_clock_release(&c->regs,&c->audioTopClock);
}
#endif
/* This only records a mapping supplied by the validated audio child. It does
 * not establish exclusive hardware ownership or retained power. */
static int ParentBindAfe(PVOID context,PVOID afe,ULONG length) {
    A04_PARENT_CONTEXT *c;int result=-1;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(afe && length==0x1000 && !c->audioAfe && !c->audio.leased && !c->audio.poisoned) {
        c->audioAfe=afe;result=0;
    } else if(!afe && !length && c->audioAfe && !c->audio.leased && !c->audio.poisoned) {
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
        if(!c->afeClockHeld && !c->afeClockPoisoned)
#endif
        {c->audioAfe=NULL;result=0;}
    }
    ParentUnlock(c);return result;
}
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
/* AUDIO_TOP_CON0 bit 2, under the parent lock and an acquired TOP gate.
 * Power and competing access must be independently verified by preflight. */
static int ParentAfeClockAcquire(void *context,uint32_t rate) {
    A04_PARENT_CONTEXT *c=context;volatile ULONG *reg;ULONG value;
    if(!c || !c->audioAfe || !c->audioTopClock.held || c->audioTopClock.poisoned ||
       !c->pmic.online || c->afeClockHeld || c->afeClockPoisoned || rate!=48000)return -1;
    reg=(volatile ULONG *)c->audioAfe;value=READ_REGISTER_ULONG(reg);
    c->afeClockHeld=1;c->afeClockRestore=(value&4u)!=0;
    if(c->afeClockRestore) {
        WRITE_REGISTER_ULONG(reg,value&~4u);
        if(READ_REGISTER_ULONG(reg)&4u){c->afeClockPoisoned=1;return -1;}
    }
    return 0;
}
static int ParentAfeClockRelease(void *context) {
    A04_PARENT_CONTEXT *c=context;volatile ULONG *reg;ULONG value;
    if(!c || !c->audioAfe || !c->pmic.online || !c->audioTopClock.held ||
       !c->afeClockHeld || c->afeClockPoisoned)return -1;
    reg=(volatile ULONG *)c->audioAfe;value=READ_REGISTER_ULONG(reg);
    if(value&4u){c->afeClockPoisoned=1;return -1;}
    if(c->afeClockRestore) {
        WRITE_REGISTER_ULONG(reg,value|4u);
        if(!(READ_REGISTER_ULONG(reg)&4u)){c->afeClockPoisoned=1;return -1;}
    }
    c->afeClockHeld=0;c->afeClockRestore=0;return 0;
}
#endif
static int ParentAudioAcquire(PVOID context,ULONG rate) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    result=a04_audio_owner_acquire(&c->audio,rate);
    A04_TRACE("A04AUD parent acquire rate=%lu result=%d pmic=%d apmixed=%d online=%d preflight=%d clock=%d codec=%d release-check=%d poisoned=%d\n",rate,result,c->pmic.online,c->apmixed!=NULL,c->audio.online,c->audio.preflight!=NULL,c->audio.clock_acquire!=NULL,c->audio.codec_acquire_muted!=NULL,c->audio.verify_release!=NULL,c->audio.poisoned);
    if(result && c->audio.poisoned)result=-2;
    ParentUnlock(c);return result;
}
static int ParentAudioUnmute(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()>APC_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    result=a04_audio_owner_unmute(&c->audio);ParentUnlock(c);return result;
}
static int ParentAudioMute(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()>APC_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    result=a04_audio_owner_mute(&c->audio);ParentUnlock(c);return result;
}
static int ParentAudioRelease(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    result=a04_audio_owner_release(&c->audio);ParentUnlock(c);return result;
}
/* Only a fully acquired exclusive audio lease may mask and clear the IRQ.
 * The preflight contract must exclude HDMI/TDM and all other AFE users. */
static int ParentAudioPrepareIrq(PVOID context,PVOID afe,ULONG length) {
#if A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC
    A04_PARENT_CONTEXT *c;volatile UCHAR *base;ULONG dac,irq,irq2,status;int result=-1;
    if(!context || !afe || length!=0x1000 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(afe!=c->audioAfe || !c->audio.online || !c->audio.leased || c->audio.poisoned ||
       !c->pmic.online || !c->audioTopClock.held || c->audioTopClock.poisoned ||
       !c->afeClockHeld || c->afeClockPoisoned)goto done;
    base=(volatile UCHAR *)afe;
    dac=READ_REGISTER_ULONG((volatile ULONG *)(base+0x10));
    /* AFE memifs with explicit DAC_CON0 enables must already be inactive. */
    if(dac & 0xdeu)goto done;
    irq=READ_REGISTER_ULONG((volatile ULONG *)(base+0x3a0));
    irq2=READ_REGISTER_ULONG((volatile ULONG *)(base+0x3f8));
    WRITE_REGISTER_ULONG((volatile ULONG *)(base+0x3a0),irq & ~0x4003u);
    WRITE_REGISTER_ULONG((volatile ULONG *)(base+0x3f8),irq2 & ~0x18u);
    WRITE_REGISTER_ULONG((volatile ULONG *)(base+0x3a8),0x2ffu);
    irq=READ_REGISTER_ULONG((volatile ULONG *)(base+0x3a0));
    irq2=READ_REGISTER_ULONG((volatile ULONG *)(base+0x3f8));
    status=READ_REGISTER_ULONG((volatile ULONG *)(base+0x3a4));
    if((irq&0x4003u) || (irq2&0x18u) || (status&0x2ffu)) {c->audio.poisoned=1;goto done;}
    result=0;
done:
    ParentUnlock(c);return result;
#else
    UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(afe);UNREFERENCED_PARAMETER(length);return -1;
#endif
}
static VOID ParentInterfaceReference(PVOID context) { WdfObjectReference((WDFDEVICE)context); }
static VOID ParentInterfaceDereference(PVOID context) { WdfObjectDereference((WDFDEVICE)context); }
static NTSTATUS ParentReadButtons(PVOID context,USHORT *value) {
    A04_PARENT_CONTEXT *c;
    if(!context || !value || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    c=ParentContext((WDFDEVICE)context);
    return a04_pwrap_owner_read_buttons(&c->pmic,value)==0 ? STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;
}
#endif
static int ParentUsbLock(void *p) { return ParentLock(p); }
static void ParentUsbUnlock(void *p) { ParentUnlock(p); }
static unsigned ParentUsbTopRead(void *p,unsigned off) { A04_PARENT_CONTEXT*c=p; return READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->topck+off)); }
static void ParentUsbTopWrite(void *p,unsigned off,unsigned v) { A04_PARENT_CONTEXT*c=p; WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->topck+off),v); }
static unsigned ParentUsbGpioRead(void *p,unsigned off) { A04_PARENT_CONTEXT*c=p; return READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->gpio+off)); }
static void ParentUsbGpioWrite(void *p,unsigned off,unsigned v) { A04_PARENT_CONTEXT*c=p; WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->gpio+off),v); }
static int ParentUsbOvercurrent(void *p) {
 A04_PARENT_CONTEXT*c=p; unsigned mode,dir,input;
 if(!c->gpio)return -1;
 mode=ParentUsbGpioRead(c,0x340);dir=ParentUsbGpioRead(c,0x10);input=ParentUsbGpioRead(c,0x210);
 /* GPIO21: Linux overcurrent IRQ starts active low. Require input and GPIO. */
 return !(mode & (7u<<3)) && !(dir & (1u<<5)) && (input & (1u<<5)) ? 0 : -1;
}
static int ParentUsbVerify(void *p) {
 A04_PARENT_CONTEXT*c=p; ULONG sel,con,pwr,rate;
 if(!c->topck || !c->apmixed)return -1;
 sel=ParentUsbTopRead(c,4); con=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x140));
 pwr=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x150));
 rate=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x144));
 /* USB78 mux = UNIVPLL/16, PLL enabled, powered and not isolated. */
 return ((sel>>20)&7)==2 && (con&0x30000001u)==0x30000001u && ((pwr&3)==1) && (rate&0x7fu)==48 && ((rate>>24)&7u)==0 && rate==READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x144)) ? 0 : -1;
}
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
static int ParentUsbAcquire(PVOID p) { return a04_usb_owner_acquire(&ParentContext((WDFDEVICE)p)->usb); }
static int ParentUsbVbus(PVOID p,int on) { return a04_usb_owner_vbus(&ParentContext((WDFDEVICE)p)->usb,on); }
static int ParentUsbHealth(PVOID p) { return a04_usb_owner_health(&ParentContext((WDFDEVICE)p)->usb); }
static int ParentUsbRelease(PVOID p) { return a04_usb_owner_release(&ParentContext((WDFDEVICE)p)->usb); }
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC && defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
static NTSTATUS ParentThermalAcquire(PVOID p,ULONG *generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);uint32_t value=0;int result;
 if(!generation || ParentLock(c))return STATUS_DEVICE_NOT_READY;
 result=a04_shared_thermal_acquire(&c->thermal,&value);ParentUnlock(c);
 if(result)return STATUS_DEVICE_NOT_READY;*generation=value;return STATUS_SUCCESS;
}
static NTSTATUS ParentThermalClock(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);uint32_t state;NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(a04_shared_thermal_valid(&c->thermal,generation) && !c->thermal.quiescent) {
  if(c->thermal.clocks_ready)result=STATUS_SUCCESS;
  else if(ParentRegRead(c,A04_SHARED_TOP,0x24,&state)) {c->thermal.poisoned=1;result=STATUS_DEVICE_HARDWARE_ERROR;}
  else {
   c->thermal.restore_gated=state&((1u<<1)|(1u<<30));
   if((!c->thermal.restore_gated || !ParentRegWrite(c,A04_SHARED_TOP,0x84,c->thermal.restore_gated)) &&
      !ParentRegRead(c,A04_SHARED_TOP,0x24,&state) && !(state&((1u<<1)|(1u<<30)))) {
    c->thermal.clocks_ready=1;c->thermal.quiescent=0;result=STATUS_SUCCESS;
   } else {c->thermal.poisoned=1;result=STATUS_DEVICE_HARDWARE_ERROR;}
  }
 }
 ParentUnlock(c);return result;
}
static NTSTATUS ParentThermalReset(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);uint32_t state;NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(a04_shared_thermal_valid(&c->thermal,generation) && c->thermal.clocks_ready && !c->thermal.quiescent) {
  if(!ParentRegWrite(c,A04_SHARED_INFRA,0x120,1) && !ParentRegWrite(c,A04_SHARED_INFRA,0x124,1) &&
     !ParentRegRead(c,A04_SHARED_INFRA,0x128,&state) && !(state&1))result=STATUS_SUCCESS;
  else {c->thermal.poisoned=1;result=STATUS_DEVICE_HARDWARE_ERROR;}
 }
 ParentUnlock(c);return result;
}
static BOOLEAN ParentThermalBegin(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);int valid;
 if(!ExAcquireRundownProtection(&c->thermalRundown))return FALSE;
 if(ParentLock(c)){ExReleaseRundownProtection(&c->thermalRundown);return FALSE;}
 valid=!a04_shared_thermal_begin(&c->thermal,generation);ParentUnlock(c);
 if(!valid)ExReleaseRundownProtection(&c->thermalRundown);return valid?TRUE:FALSE;
}
static VOID ParentThermalEnd(PVOID p)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);
 /* End remains legal after admission closes; ParentStop waits for rundown. */
 ExAcquireFastMutex(&c->pwrapLock);a04_shared_thermal_end(&c->thermal);ExReleaseFastMutex(&c->pwrapLock);
 ExReleaseRundownProtection(&c->thermalRundown);
}
static NTSTATUS ParentThermalReadAnalog(PVOID p,ULONG generation,ULONG *value)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(!value || ParentLock(c))return result;
 if(a04_shared_thermal_valid(&c->thermal,generation) && c->thermal.inflight && c->apmixed && c->apmixedLength>=0x608) {
  *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604));
  if(*value!=MAXULONG)result=STATUS_SUCCESS;else c->thermal.poisoned=1;
 }
 ParentUnlock(c);return result;
}
static NTSTATUS ParentThermalWriteAnalog(PVOID p,ULONG generation,ULONG value)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);volatile ULONG *reg;ULONG before,after;NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(a04_shared_thermal_valid(&c->thermal,generation) && c->thermal.inflight && c->apmixed && c->apmixedLength>=0x608) {
  reg=(volatile ULONG *)((volatile UCHAR *)c->apmixed+0x604);before=READ_REGISTER_ULONG(reg);
  if(before!=MAXULONG && value==(before&~0x30u)) {
   WRITE_REGISTER_ULONG(reg,value);after=READ_REGISTER_ULONG(reg);
   if(after==value)result=STATUS_SUCCESS;else c->thermal.poisoned=1;
  } else c->thermal.poisoned=1;
 }
 ParentUnlock(c);return result;
}
static NTSTATUS ParentThermalQuiescent(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);int result;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 result=a04_shared_thermal_quiescent(&c->thermal,generation);ParentUnlock(c);
 return result?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
}
static NTSTATUS ParentThermalRelease(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);int result;
 if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(!a04_shared_thermal_valid(&c->thermal,generation) || c->thermal.inflight) {ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 if(c->thermal.clocks_ready && !c->thermal.quiescent) {c->thermal.poisoned=1;ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 if(c->thermal.clocks_ready && c->thermal.restore_gated) {
  uint32_t state;
  if(ParentRegWrite(c,A04_SHARED_TOP,0x54,c->thermal.restore_gated) ||
     ParentRegRead(c,A04_SHARED_TOP,0x24,&state) ||
     (state&c->thermal.restore_gated)!=c->thermal.restore_gated) {
   c->thermal.poisoned=1;ParentUnlock(c);return STATUS_DEVICE_HARDWARE_ERROR;
  }
 }
 result=a04_shared_thermal_release(&c->thermal,generation);ParentUnlock(c);
 return result?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
}
#endif
static void ParentStop(A04_PARENT_CONTEXT *c,int can_access)
{
    /* Both owners share this mutex. Close admission in one critical section
     * before any mapping is released; neither client can enter in between. */
    InterlockedExchange(&c->admissionClosed,1);
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(!c->thermalRundownClosed){ExWaitForRundownProtectionRelease(&c->thermalRundown);c->thermalRundownClosed=TRUE;}
#endif
    ExAcquireFastMutex(&c->pwrapLock);
    c->regs.online=0;
    c->pmic.online=0;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    a04_shared_thermal_stop(&c->thermal);
#endif
    a04_audio_owner_stop(&c->audio);
    c->usb.online=0;
    /* Surprise removal may already make MMIO unsafe. On ordered D0 exit,
     * remove VBUS while mappings remain valid; revoke the lease in either case. */
    if(c->usb.leased) {
        if(can_access && !InterlockedCompareExchange(&c->removed,0,0) && c->gpio &&
           !A04Gpio22Vbus(&c->usb.gpio,0) && !c->usb.otg_held)c->usb.leased=0;
        c->usb.poisoned=1;
    }
    ExReleaseFastMutex(&c->pwrapLock);
}
static NTSTATUS ParentUnmap(A04_PARENT_CONTEXT *c)
{
    ParentStop(c,1);
    /* Failed/partial audio ownership cannot be interpreted as quiescent.
     * Retain mappings; this is not proof PnP retains physical resources. */
    if(c->audio.poisoned || c->audio.leased || c->audioAfe || c->usb.leased)return STATUS_DEVICE_BUSY;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(c->thermal.leased || c->thermal.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
    if(c->codecDomain.held || c->codecDomain.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
    if(c->afeClockHeld || c->afeClockPoisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
    if(c->audioTopClock.held || c->audioTopClock.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
    if(c->speakerClock.held)return STATUS_DEVICE_BUSY;
#endif
    if(c->apmixed){MmUnmapIoSpace(c->apmixed,c->apmixedLength);c->apmixed=NULL;c->apmixedLength=0;}
    if(c->pwrap){MmUnmapIoSpace(c->pwrap,0x1000);c->pwrap=NULL;}
    if(c->gpio){MmUnmapIoSpace(c->gpio,0x1000);c->gpio=NULL;}
    if(c->infra){MmUnmapIoSpace(c->infra,0x1000);c->infra=NULL;}
    if(c->topck){MmUnmapIoSpace(c->topck,0x1000);c->topck=NULL;}
    return STATUS_SUCCESS;
}
NTSTATUS ParentPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    A04_PARENT_CONTEXT *c=ParentContext(dev);struct a04_parent_resource r[5],t[5];
    struct a04_parent_mapping mapping;ULONG i,n=0;PHYSICAL_ADDRESS pa;
#if A04_SHARED_APMIXED_RESOURCE
    uint64_t apmixed;
#endif
    if(c->topck||c->infra||c->gpio||c->pwrap||c->apmixed)return STATUS_INVALID_DEVICE_STATE;
    if(!raw || !translated || WdfCmResourceListGetCount(raw)!=WdfCmResourceListGetCount(translated))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<WdfCmResourceListGetCount(translated);++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR original=WdfCmResourceListGetDescriptor(raw,i);
        if(!d || !original || d->Type!=original->Type)return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->Type!=CmResourceTypeMemory)continue;
        if(n>=5)return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->u.Memory.Start.QuadPart<0 || original->u.Memory.Start.QuadPart<0)return STATUS_DEVICE_CONFIGURATION_ERROR;
        r[n].base=(uint64_t)original->u.Memory.Start.QuadPart;r[n].length=original->u.Memory.Length;
        r[n].exclusive=original->ShareDisposition==CmResourceShareDeviceExclusive ? 1u:0u;
        t[n].base=(uint64_t)d->u.Memory.Start.QuadPart;t[n].length=d->u.Memory.Length;
        t[n].exclusive=d->ShareDisposition==CmResourceShareDeviceExclusive ? 1u:0u;++n;
    }
#if A04_SHARED_APMIXED_RESOURCE
    if(a04_parent_translate_audio_resources(r,t,n,&mapping,&apmixed))return STATUS_DEVICE_CONFIGURATION_ERROR;
#else
    if(a04_parent_translate_resources(r,t,n,&mapping))return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
    pa.QuadPart=(LONGLONG)mapping.topck;c->topck=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->topck)goto fail;
    pa.QuadPart=(LONGLONG)mapping.infra;c->infra=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->infra)goto fail;
    pa.QuadPart=(LONGLONG)mapping.gpio;c->gpio=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->gpio)goto fail;
    pa.QuadPart=(LONGLONG)mapping.pwrap;c->pwrap=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->pwrap)goto fail;
#if A04_SHARED_APMIXED_RESOURCE
    for(i=0;i<n;++i)if(r[i].base==0x10018000u)break;
    if(i==n)goto fail;
    c->apmixedLength=t[i].length;
    pa.QuadPart=(LONGLONG)apmixed;c->apmixed=MmMapIoSpace(pa,c->apmixedLength,MmNonCached);
    if(!c->apmixed)goto fail;
#endif
    /* Mapping alone does not admit clients; D0 must establish readiness. */
    return STATUS_SUCCESS;
fail:
    ParentUnmap(c);return STATUS_INSUFFICIENT_RESOURCES;
}
NTSTATUS ParentRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
    UNREFERENCED_PARAMETER(translated);
    /* Revoke admission and drain active operations before unmapping. */
    return ParentUnmap(ParentContext(dev));
}
VOID ParentSurprise(WDFDEVICE dev)
{
    /* Offline admission only: this cannot prevent a bus abort from removal
     * concurrent with an already executing hardware access. */
    InterlockedExchange(&ParentContext(dev)->removed,1);
    ParentStop(ParentContext(dev),0);
}
NTSTATUS ParentD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{
    UNREFERENCED_PARAMETER(target);ParentStop(ParentContext(dev),1);return STATUS_SUCCESS;
}
NTSTATUS ParentD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
    UNREFERENCED_PARAMETER(previous);
#if A04_SHARED_BUTTON_D0_EXPERIMENT
    A04_PARENT_CONTEXT *c=ParentContext(dev);NTSTATUS result=STATUS_DEVICE_NOT_READY;ULONG init,state;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(c->thermalRundownClosed){ExReInitializeRundownProtection(&c->thermalRundown);c->thermalRundownClosed=FALSE;}
#endif
    /* 1=closed, 2=starting, 0=open. Stop can cancel a start before the
     * final compare-exchange; surprise removal prevents any later restart. */
    if (InterlockedCompareExchange(&c->removed,0,0) ||
        InterlockedCompareExchange(&c->admissionClosed,2,1)!=1) return result;
    ExAcquireFastMutex(&c->pwrapLock);
    if (InterlockedCompareExchange(&c->removed,0,0) ||
        InterlockedCompareExchange(&c->admissionClosed,0,0)!=2 ||
        !c->topck || !c->infra || !c->gpio || !c->pwrap || c->pmic.bus.poisoned ||
        !c->pmic.bus.read32 || !c->pmic.bus.write32 || !c->pmic.bus.delay || !c->pmic.bus.attempts) goto done;
    /* Existing button provider uses these WACS2 readiness observations.
     * The optional trace performs only one PMIC read after these checks. */
    init=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+0x9c));
    state=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+0xa4));
    if (!(init&1u) || ((state>>16)&7u)!=0 ||
        InterlockedCompareExchange(&c->removed,0,0) ||
        InterlockedCompareExchange(&c->admissionClosed,0,0)!=2) goto done;
#if A04_AUDIO_PMIC_TRACE
    {
        uint16_t raw=0;
        if(a04_pmic_spk_clock_status(&c->pmic.bus,&raw)) {
            A04_TRACE("A04AUD parent d0 pmic-clock read failed; reject\n");goto done;
        }
        A04_TRACE("A04AUD parent d0 TOP_CKPDN1=%04x; observation only\n",(unsigned)raw);
    }
#endif
    c->pmic.online=1;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    /* No known provider proves release by EL3 or every AUXADC consumer. */
    c->thermal.online=1;c->thermal.firmware_released=0;c->thermal.adc_exclusive=0;
#endif
    c->usb.online=(c->apmixed && !c->usb.poisoned) ? 1 : 0;
#if A04_SHARED_TOUCH_D0_EXPERIMENT
    c->regs.online=c->regs.poisoned ? 0 : 1;
#endif
    if (InterlockedCompareExchange(&c->admissionClosed,0,2)==2) result=STATUS_SUCCESS;
    else { c->pmic.online=0; c->usb.online=0; c->regs.online=0; }
done:
    if (!NT_SUCCESS(result)) { c->pmic.online=0; c->usb.online=0; c->regs.online=0;(void)InterlockedCompareExchange(&c->admissionClosed,1,2); }
    A04_TRACE("A04AUD parent d0 result=%08lx poisoned=%d; audio lease remains gated\n",(ULONG)result,c->pmic.bus.poisoned);
    ExReleaseFastMutex(&c->pwrapLock);return result;
#else
    UNREFERENCED_PARAMETER(dev);
    /* Existing direct owners, boot ordering and shared lease teardown have not
     * been migrated. An ACPI resource assignment alone is not ownership. */
    return STATUS_DEVICE_NOT_READY;
#endif
}
#if defined(A04_SHARED_CHILD_PROBE) && A04_SHARED_CHILD_PROBE
/* Compile-only topology. PnP arbitrates these exclusive child ranges; the
 * parent must not map them. Parent D0 remains gated, and legacy ACPI devices
 * must be disabled in a future mutually exclusive firmware configuration. */
typedef struct { ULONG base; } A04_CHILD_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_CHILD_CONTEXT,ChildContext)
EVT_WDF_DEVICE_RESOURCE_REQUIREMENTS_QUERY ChildRequirements;
NTSTATUS ChildRequirements(WDFDEVICE child,WDFIORESREQLIST requirements)
{
    A04_CHILD_CONTEXT *c=ChildContext(child); WDFIORESLIST list;
    IO_RESOURCE_DESCRIPTOR descriptor; NTSTATUS status;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(c->base==0xfffffffeu) {
        static const ULONG thermalBase[2]={0x1100d000u,0x11003000u};unsigned i;
        status=WdfIoResourceListCreate(requirements,WDF_NO_OBJECT_ATTRIBUTES,&list);
        if(!NT_SUCCESS(status))return status;
        for(i=0;i<2;i++) {
            RtlZeroMemory(&descriptor,sizeof(descriptor));descriptor.Type=CmResourceTypeMemory;
            descriptor.ShareDisposition=CmResourceShareDeviceExclusive;descriptor.Flags=CM_RESOURCE_MEMORY_READ_WRITE;
            descriptor.u.Memory.Length=0x1000;descriptor.u.Memory.Alignment=1;
            descriptor.u.Memory.MinimumAddress.QuadPart=thermalBase[i];descriptor.u.Memory.MaximumAddress.QuadPart=thermalBase[i]+0xfff;
            status=WdfIoResourceListAppendDescriptor(list,&descriptor);
            if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
        }
        status=WdfIoResourceRequirementsListAppendIoResList(requirements,list);
        if(!NT_SUCCESS(status))WdfObjectDelete(list);return status;
    }
#endif
#if defined(A04_AUDIO_CHILD_RESOURCES) && A04_AUDIO_CHILD_RESOURCES
    if(c->base==0xffffffffu) {
        /* Diagnostic raw GSI candidate. IO_RESOURCE_DESCRIPTOR does not
         * carry polarity; actual controller routing is not verified. */
        status=WdfIoResourceListCreate(requirements,WDF_NO_OBJECT_ATTRIBUTES,&list);
        if(!NT_SUCCESS(status))return status;
        RtlZeroMemory(&descriptor,sizeof(descriptor));
        descriptor.Type=CmResourceTypeMemory;descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
        descriptor.Flags=CM_RESOURCE_MEMORY_READ_WRITE;descriptor.u.Memory.Alignment=1;
        descriptor.u.Memory.Length=0x1000;descriptor.u.Memory.MinimumAddress.QuadPart=0x11140000;
        descriptor.u.Memory.MaximumAddress.QuadPart=0x11140fff;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
        descriptor.u.Memory.Length=0x9000;descriptor.u.Memory.MinimumAddress.QuadPart=0x11141000;
        descriptor.u.Memory.MaximumAddress.QuadPart=0x11149fff;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
        RtlZeroMemory(&descriptor,sizeof(descriptor));
        descriptor.Type=CmResourceTypeInterrupt;descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
        descriptor.Flags=CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        descriptor.u.Interrupt.MinimumVector=152;descriptor.u.Interrupt.MaximumVector=152;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
        status=WdfIoResourceRequirementsListAppendIoResList(requirements,list);
        if(!NT_SUCCESS(status))WdfObjectDelete(list);
        return status;
    }
#endif
    if(c->base!=0x10002000u && c->base!=0x11009000u && c->base!=0x11100000u)return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=WdfIoResourceListCreate(requirements,WDF_NO_OBJECT_ATTRIBUTES,&list);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&descriptor,sizeof(descriptor));
    descriptor.Type=CmResourceTypeMemory; descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
    descriptor.Flags=CM_RESOURCE_MEMORY_READ_WRITE;
    descriptor.u.Memory.Length=c->base==0x11100000u?0x10000:0x1000; descriptor.u.Memory.Alignment=1;
    descriptor.u.Memory.MinimumAddress.QuadPart=c->base;
    descriptor.u.Memory.MaximumAddress.QuadPart=(LONGLONG)c->base+descriptor.u.Memory.Length-1;
    status=WdfIoResourceListAppendDescriptor(list,&descriptor);
    if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
    if(c->base==0x11100000u) {
        /* USB20 PHY plus GIC SPI72 -> GSIV104. This synthetic requirements
         * fixture still needs target PnP polarity/routing validation. */
        descriptor.u.Memory.MinimumAddress.QuadPart=0x11110000u;
        descriptor.u.Memory.MaximumAddress.QuadPart=0x1111ffffu;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
        RtlZeroMemory(&descriptor,sizeof(descriptor));descriptor.Type=CmResourceTypeInterrupt;
        descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
        descriptor.Flags=CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        descriptor.u.Interrupt.MinimumVector=104;descriptor.u.Interrupt.MaximumVector=104;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
    }
    status=WdfIoResourceRequirementsListAppendIoResList(requirements,list);
    if(!NT_SUCCESS(status))WdfObjectDelete(list);
    return status;
}
static NTSTATUS ParentAddProbeChild(WDFDEVICE parent, PCWSTR id, PCWSTR instance, ULONG base)
{
    PWDFDEVICE_INIT init; WDFDEVICE child; WDF_QUERY_INTERFACE_CONFIG query;
    UNICODE_STRING name; NTSTATUS status; WDF_OBJECT_ATTRIBUTES attr; WDF_PDO_EVENT_CALLBACKS callbacks;
    init=WdfPdoInitAllocate(parent);if(!init)return STATUS_INSUFFICIENT_RESOURCES;
    RtlInitUnicodeString(&name,id);status=WdfPdoInitAssignDeviceID(init,&name);
    if(!NT_SUCCESS(status))goto fail;
    status=WdfPdoInitAddHardwareID(init,&name);if(!NT_SUCCESS(status))goto fail;
    RtlInitUnicodeString(&name,instance);status=WdfPdoInitAssignInstanceID(init,&name);
    if(!NT_SUCCESS(status))goto fail;
    WDF_PDO_EVENT_CALLBACKS_INIT(&callbacks);
    callbacks.EvtDeviceResourceRequirementsQuery=ChildRequirements;
    WdfPdoInitSetEventCallbacks(init,&callbacks);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_CHILD_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&child);
    if(!NT_SUCCESS(status))goto fail;
    ChildContext(child)->base=base;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;
    status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
#if A04_SHARED_THERMAL_DIAGNOSTIC
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedThermalInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
#endif
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedUsbInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;
    status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedAudioInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;
    status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    status=WdfFdoAddStaticChild(parent,child);
    if(!NT_SUCCESS(status))WdfObjectDelete(child);
    return status;
fail:
    if(init)WdfDeviceInitFree(init);
    return status;
}
#endif
NTSTATUS ParentAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;WDF_OBJECT_ATTRIBUTES attr;WDFDEVICE dev;NTSTATUS status;
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
    A04_LEGACY_SHARED_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;
    A04_SHARED_INTERFACE inputIface;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    A04_SHARED_THERMAL_INTERFACE thermalIface;
#endif
    A04_SHARED_AUDIO_INTERFACE audioIface; A04_SHARED_USB_INTERFACE usbIface;
#endif
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=ParentPrepare;pnp.EvtDeviceReleaseHardware=ParentRelease;pnp.EvtDeviceD0Entry=ParentD0;
    pnp.EvtDeviceD0Exit=ParentD0Exit;pnp.EvtDeviceSurpriseRemoval=ParentSurprise;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_PARENT_CONTEXT);
    attr.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfDeviceCreate(&init,&attr,&dev);
    if(!NT_SUCCESS(status))return status;
    {
        A04_PARENT_CONTEXT *c=ParentContext(dev);
        ExInitializeFastMutex(&c->pwrapLock);
#if A04_SHARED_THERMAL_DIAGNOSTIC
        ExInitializeRundownProtection(&c->thermalRundown);
#endif
        c->admissionClosed=1;
        c->pmic.lock_context=c;c->pmic.lock=ParentLock;c->pmic.unlock=ParentUnlock;
        c->pmic.bus.context=c;c->pmic.bus.read32=ParentPwrapRead;c->pmic.bus.write32=ParentPwrapWrite;
        c->pmic.bus.delay=ParentPwrapDelay;c->pmic.bus.attempts=1000;
        c->regs.context=c;c->regs.lock=ParentRegLock;c->regs.unlock=ParentRegUnlock;
        c->regs.read32=ParentRegRead;c->regs.write32=ParentRegWrite;
#if A04_SHARED_MT6392_DIAGNOSTIC
        c->classd.context=c;c->classd.supports=ParentCodecSupports;c->classd.update_bits=ParentCodecUpdate;c->classd.read16=ParentCodecRead;c->classd.delay_us=ParentCodecDelay;
        c->audio.context=c;c->audio.codec_acquire_muted=ParentCodecStart;
        c->audio.codec_mute=ParentCodecMute;c->audio.codec_release_quiescent=ParentCodecStop;
#if defined(A04_MT6392_UNMUTE_DIAGNOSTIC) && A04_MT6392_UNMUTE_DIAGNOSTIC
        c->audio.codec_unmute=ParentCodecUnmute;
#endif
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
        c->audio.context=c;c->audio.top_clock_acquire=ParentTopAudioAcquire;
        c->audio.top_clock_release=ParentTopAudioRelease;
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
        c->audio.clock_acquire=ParentAfeClockAcquire;c->audio.clock_release=ParentAfeClockRelease;
#endif
        c->usb.context=c;c->usb.lock=ParentUsbLock;c->usb.unlock=ParentUsbUnlock;
        c->usb.verify_upstream=ParentUsbVerify;c->usb.verify_overcurrent=ParentUsbOvercurrent;c->usb.top_read=ParentUsbTopRead;c->usb.top_write=ParentUsbTopWrite;
        c->usb.pmic=&c->pmic.bus;c->usb.gpio.context=c;c->usb.gpio.read32=ParentUsbGpioRead;c->usb.gpio.write32=ParentUsbGpioWrite;
    }
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
    RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);iface.Header.Version=1;
    iface.Header.Context=dev;iface.Header.InterfaceReference=ParentInterfaceReference;
    iface.Header.InterfaceDereference=ParentInterfaceDereference;iface.ReadButtons=ParentReadButtons;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&GUID_A04_MT8167_SHARED_INTERFACE,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&inputIface,sizeof(inputIface));
    inputIface.Size=sizeof(inputIface);inputIface.Version=A04_SHARED_INTERFACE_VERSION;
    inputIface.Context=dev;inputIface.InterfaceReference=ParentInterfaceReference;
    inputIface.InterfaceDereference=ParentInterfaceDereference;
    inputIface.TouchUpdate=ParentTouchUpdate;inputIface.TouchWrite=ParentTouchWrite;
    inputIface.TouchPen=ParentTouchPen;inputIface.ButtonPmicRead=ParentButtonRead;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&inputIface,&A04SharedInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    RtlZeroMemory(&thermalIface,sizeof(thermalIface));thermalIface.Header.Size=sizeof(thermalIface);
    thermalIface.Header.Version=A04_SHARED_THERMAL_INTERFACE_VERSION;thermalIface.Header.Context=dev;
    thermalIface.Header.InterfaceReference=ParentInterfaceReference;thermalIface.Header.InterfaceDereference=ParentInterfaceDereference;
    thermalIface.Acquire=ParentThermalAcquire;thermalIface.Clock=ParentThermalClock;thermalIface.Reset=ParentThermalReset;
    thermalIface.Begin=ParentThermalBegin;thermalIface.End=ParentThermalEnd;
    thermalIface.ReadAnalog=ParentThermalReadAnalog;thermalIface.WriteAnalog=ParentThermalWriteAnalog;
    thermalIface.Quiescent=ParentThermalQuiescent;thermalIface.Release=ParentThermalRelease;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&thermalIface,&A04SharedThermalInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
#endif
    RtlZeroMemory(&audioIface,sizeof(audioIface));
    audioIface.Header.Size=sizeof(audioIface);audioIface.Header.Version=A04_SHARED_AUDIO_INTERFACE_VERSION;
    audioIface.Header.Context=dev;audioIface.Header.InterfaceReference=ParentInterfaceReference;
    audioIface.Header.InterfaceDereference=ParentInterfaceDereference;
    audioIface.BindAfe=ParentBindAfe;audioIface.Mute=ParentAudioMute;audioIface.UnmuteMin=ParentAudioUnmute;audioIface.AcquireMuted=ParentAudioAcquire;audioIface.ReleaseQuiescent=ParentAudioRelease;audioIface.PrepareMaskedIrq=ParentAudioPrepareIrq;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&audioIface,&A04SharedAudioInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&usbIface,sizeof(usbIface));usbIface.Header.Size=sizeof(usbIface);usbIface.Header.Version=A04_SHARED_USB_INTERFACE_VERSION;
    usbIface.Header.Context=dev;usbIface.Header.InterfaceReference=ParentInterfaceReference;usbIface.Header.InterfaceDereference=ParentInterfaceDereference;
    usbIface.Acquire=ParentUsbAcquire;usbIface.SetVbus=ParentUsbVbus;usbIface.Health=ParentUsbHealth;usbIface.Release=ParentUsbRelease;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&usbIface,&A04SharedUsbInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
#endif
#if defined(A04_SHARED_CHILD_PROBE) && A04_SHARED_CHILD_PROBE
    if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"MT6392\\Buttons",L"0",0x10002000u);
    if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"MT8167\\BU21025",L"0",0x11009000u);
    if(!NT_SUCCESS(status))return status;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    status=ParentAddProbeChild(dev,L"MT8167\\Thermal",L"0",0xfffffffeu);
    if(!NT_SUCCESS(status))return status;
#endif
    status=ParentAddProbeChild(dev,L"MT8167\\USB20",L"0",0x11100000u);
    if(!NT_SUCCESS(status))return status;
    /* The audio sentinel remains unavailable unless the diagnostic flag is set. */
    status=ParentAddProbeChild(dev,L"MT8167\\AFE",L"0",0xffffffffu);
#endif
    return status;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,ParentAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}

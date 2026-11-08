/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental PnP parent. D0 rejects by default; the opt-in button D0
 * experiment checks WACS2 readiness and admits the fixed button transaction.
 * Never deploy while existing button/touch drivers claim the same resources. */
#include <initguid.h>
#include <ntddk.h>
#include <wdf.h>
#include <wdmguid.h>
#include "parent_resources.h"
#include "usb_admission.h"
#include "pwrap_owner.h"
#ifndef MT6392_RTC_CALENDAR
#define MT6392_RTC_CALENDAR 0
#endif
#if MT6392_RTC_CALENDAR
#include "../../system/mt6392-rtc/rtc_interface.h"
#if !A04_SHARED_QUERY_EXPERIMENTAL || !A04_SHARED_CHILD_PROBE || !A04_SHARED_BUTTON_D0_EXPERIMENT
#error RTC calendar requires the existing shared owner and child topology
#endif
#endif
#include "register_owner.h"
#include "client_interface.h"
#include "audio_interface.h"
#ifndef A04_SHARED_SD_POWER
#define A04_SHARED_SD_POWER 0
#endif
#if A04_SHARED_SD_POWER
#include "../../storage/mt8167-sd/power_owner.h"
#include "../../storage/mt8167-sd/power_interface.h"
#if !defined(A04_SHARED_BUTTON_D0_EXPERIMENT) || !A04_SHARED_BUTTON_D0_EXPERIMENT || !defined(A04_SHARED_APMIXED_RESOURCE) || !A04_SHARED_APMIXED_RESOURCE
#error SD power requires the single active PWRAP/TOP/APMIXED parent owner
#endif
#endif
#ifndef A04_SHARED_FIRST_PLAYBACK
#define A04_SHARED_FIRST_PLAYBACK 0
#endif
#if defined(A04_SHARED_ENGINE_PCM) && A04_SHARED_ENGINE_PCM
#if !A04_SHARED_FIRST_PLAYBACK || !A04_SHARED_VOICE_PCM_DIAGNOSTIC
#error engine PCM requires the retained first-playback power/codec provider
#endif
#endif
#if A04_SHARED_FIRST_PLAYBACK
#if !defined(A04_SHARED_DIGITAL_PLAYBACK) || !A04_SHARED_DIGITAL_PLAYBACK || !defined(A04_MT6392_CLK_ALIAS_EXPERIMENT) || !A04_MT6392_CLK_ALIAS_EXPERIMENT || !defined(A04_MT6392_UNMUTE_DIAGNOSTIC) || !A04_MT6392_UNMUTE_DIAGNOSTIC || !defined(A04_SHARED_VOICE_PCM_DIAGNOSTIC) || !A04_SHARED_VOICE_PCM_DIAGNOSTIC
#error first playback requires digital playback, PMIC clock aliases, unmute and voice service
#endif
#if defined(A04_SHARED_VOICE_DIAGNOSTIC) && A04_SHARED_VOICE_DIAGNOSTIC
#error first playback uses the explicit StartVoice service, not unmute-side voice startup
#endif
#endif
#ifndef A04_SHARED_DIGITAL_PLAYBACK
#define A04_SHARED_DIGITAL_PLAYBACK 0
#endif
#if A04_SHARED_DIGITAL_PLAYBACK
#if !defined(A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC) || !A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC || !defined(A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC) || !A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
#error digital playback requires the shared AFE clock and MT8167 codec domain
#endif
#include "digital_playback.h"
#endif
#include "wifi_interface.h"
#include "audio_owner.h"
#ifndef A04_SHARED_CAPTURE
#define A04_SHARED_CAPTURE 0
#endif
#if A04_SHARED_CAPTURE
#include "capture_input_owner.h"
#include "capture_interface.h"
#if !A04_SHARED_ENGINE_PCM || !A04_SHARED_FIRST_PLAYBACK
#error Capture requires the matched retained Engine parent profile
#endif
#endif
#include "usb_interface.h"
#include "usb_owner.h"
#ifndef A04_SHARED_USB_ONLY
#define A04_SHARED_USB_ONLY 0
#endif
#ifndef A04_USB_GPIO22_SOURCE_EXPERIMENT
#define A04_USB_GPIO22_SOURCE_EXPERIMENT 0
#endif
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
#include "../../usb/mt8167-usb20/board_pins.h"
#endif
#if A04_USB_GPIO22_SOURCE_EXPERIMENT && (!defined(A04_SHARED_QUERY_EXPERIMENTAL) || !A04_SHARED_QUERY_EXPERIMENTAL || !defined(A04_SHARED_CHILD_PROBE) || !A04_SHARED_CHILD_PROBE || !defined(A04_SHARED_BUTTON_D0_EXPERIMENT) || !A04_SHARED_BUTTON_D0_EXPERIMENT || !defined(A04_SHARED_APMIXED_RESOURCE) || !A04_SHARED_APMIXED_RESOURCE)
#error GPIO22 source experiment requires shared query, child, D0 and APMIXED
#endif
#ifndef A04_SHARED_THERMAL_DIAGNOSTIC
#define A04_SHARED_THERMAL_DIAGNOSTIC 0
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
#include "thermal_interface.h"
#include "thermal_owner.h"
#include "adc_observation.h"
#endif
#ifndef A04_SHARED_THERMAL_NATIVE_EXPERIMENT
#define A04_SHARED_THERMAL_NATIVE_EXPERIMENT 0
#endif
#ifndef A04_SHARED_SD_POWER
#define A04_SHARED_SD_POWER 0
#endif
#ifndef A04_SHARED_GPU_THERMAL_PROFILE
#define A04_SHARED_GPU_THERMAL_PROFILE 0
#endif
#if A04_SHARED_GPU_THERMAL_PROFILE && (!A04_SHARED_THERMAL_NATIVE_EXPERIMENT || !A04_SHARED_THERMAL_DIAGNOSTIC || !A04_SHARED_SD_POWER || !A04_SHARED_GPU_PLATFORM || !A04_SHARED_GPU_DOMAIN_SPM)
#error Parent1101 requires native PIO thermal and the SD-on GPU SPM profile
#endif
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT && !A04_SHARED_GPU_THERMAL_PROFILE
#error Legacy thermal parents are frozen: use the matched1101 RTC SD1 profile
#endif
#if A04_SHARED_GPU_THERMAL_PROFILE && (!MT6392_RTC_CALENDAR || !A04_SHARED_CAPTURE)
#error Parent1101 requires the retained RTC calendar and capture1026
#endif
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
#include "thermal_parent_pio_interface.h"
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
#ifndef A04_SHARED_VOICE_DIAGNOSTIC
#define A04_SHARED_VOICE_DIAGNOSTIC 0
#endif
#ifndef A04_SHARED_VOICE_PCM_DIAGNOSTIC
#define A04_SHARED_VOICE_PCM_DIAGNOSTIC 0
#endif
#if A04_SHARED_VOICE_DIAGNOSTIC && (!A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC || !defined(A04_MT6392_UNMUTE_DIAGNOSTIC) || !A04_MT6392_UNMUTE_DIAGNOSTIC)
#error Voice diagnostic requires codec domain and Class D diagnostic unmute
#endif
#if A04_SHARED_VOICE_PCM_DIAGNOSTIC && !A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
#error voice PCM requires codec domain diagnostic
#endif
#if A04_SHARED_VOICE_DIAGNOSTIC || A04_SHARED_VOICE_PCM_DIAGNOSTIC
#include "codec_voice.h"
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
#ifndef A04_SHARED_WIFI_RUNTIME
#define A04_SHARED_WIFI_RUNTIME 0
#endif
#if A04_SHARED_WIFI_RUNTIME != 0 && A04_SHARED_WIFI_RUNTIME != 1
#error A04_SHARED_WIFI_RUNTIME must be 0 or 1
#endif
#if A04_SHARED_WIFI_RUNTIME && A04_SHARED_USB_ONLY
#error Wi-Fi runtime requires the integrated parent child namespace
#endif
#if A04_SHARED_WIFI_RUNTIME && (!defined(A04_MT6392_RTC32K_DIAGNOSTIC) || !A04_MT6392_RTC32K_DIAGNOSTIC || !defined(A04_SHARED_WIFI_IRQ_DIAGNOSTIC) || !A04_SHARED_WIFI_IRQ_DIAGNOSTIC)
#error Wi-Fi runtime requires RTC32K and exclusive child MSDC2 IRQ resources
#endif
#ifndef A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC
#define A04_SHARED_WIFI_HANDOFF_DIAGNOSTIC 0
#endif
#ifndef A04_SHARED_WIFI_IRQ_DIAGNOSTIC
#define A04_SHARED_WIFI_IRQ_DIAGNOSTIC 0
#endif
#if A04_SHARED_WIFI_IRQ_DIAGNOSTIC && (!defined(A04_SHARED_CHILD_PROBE) || !A04_SHARED_CHILD_PROBE)
#error Wi-Fi IRQ diagnostic requires the shared child topology
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
#ifndef A04_SHARED_GPU_PLATFORM
#define A04_SHARED_GPU_PLATFORM 0
#endif
#if A04_SHARED_GPU_PLATFORM != 0 && A04_SHARED_GPU_PLATFORM != 1
#error A04_SHARED_GPU_PLATFORM must be zero or one
#endif
#if A04_SHARED_GPU_PLATFORM
#if !A04_SHARED_QUERY_EXPERIMENTAL || !A04_SHARED_BUTTON_D0_EXPERIMENT
#error GPU platform service requires the existing shared query and actual parent D0 profile
#endif
#include "gpu_domain_owner.h"
#include "gpu_platform_interface.h"
#endif
#ifndef A04_SHARED_GPU_DOMAIN_SPM
#define A04_SHARED_GPU_DOMAIN_SPM 0
#endif
#if A04_SHARED_GPU_DOMAIN_SPM && (!A04_SHARED_GPU_PLATFORM || !A04_SHARED_APMIXED_RESOURCE)
#error SPM profile requires GPU platform and matched APMIXED parent
#endif
typedef struct {
#if A04_SHARED_SD_POWER
 struct a04_sd_power sdPower;
#endif
 PVOID topck,infra,gpio,pwrap,apmixed; ULONG apmixedLength; FAST_MUTEX pwrapLock; volatile LONG admissionClosed,removed; struct a04_pwrap_owner pmic; struct a04_register_owner regs; struct a04_audio_owner audio; struct a04_usb_owner usb; ULONG wifiGeneration; int wifiActive;
#if A04_SHARED_WIFI_RUNTIME
struct a04_rtc32k_lease wifiRtc;
#endif
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
struct a04_codec_domain codecDomain;
#if A04_SHARED_VOICE_DIAGNOSTIC || A04_SHARED_VOICE_PCM_DIAGNOSTIC
struct a04_codec_voice voice;
#endif
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
struct a04_shared_thermal_owner thermal;
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
    PHYSICAL_ADDRESS thermalAssigned[2];
    PVOID thermalPioMappings[2];volatile LONG thermalPioUsed;ULONGLONG thermalPioDeadline;
#endif
EX_RUNDOWN_REF thermalRundown;BOOLEAN thermalRundownClosed;
#endif
#if A04_SHARED_GPU_PLATFORM
struct a04_gpu_domain_owner gpuDomain;
#if A04_SHARED_GPU_DOMAIN_SPM
PVOID gpuSpm;
#endif
#endif
PVOID audioAfe;
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
int afeClockHeld,afeClockRestore,afeClockPoisoned;
#if A04_SHARED_DIGITAL_PLAYBACK
struct a04_digital_playback digitalPlayback;
#if A04_SHARED_FIRST_PLAYBACK
int audioPowerPinned,trimCaptured;uint16_t trimCode;
#endif
#endif
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
#if A04_SHARED_CAPTURE
struct a04_capture_input_owner captureInput;
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
#ifndef A04_SHARED_ACCDET_READ
#define A04_SHARED_ACCDET_READ 0
#endif
#if A04_SHARED_ACCDET_READ
#include "../../input/mt8167-accdet/parent.inc"
#endif
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
/* Audio operations share the PWRAP/register mutex. The explicit retained
 * first-playback build wires the board provider at ParentAdd/D0; legacy
 * diagnostic builds leave the acquisition prerequisites unset. */
#if A04_SHARED_MT6392_DIAGNOSTIC
/* Called under ParentLock by audio_owner; no nested PWRAP lock. The board
 * first-playback provider supplies readiness and pinned power/clock leases;
 * all fixed-mask PMIC updates use the existing serialized WACS2 writer. */
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
#if A04_SHARED_FIRST_PLAYBACK
    if(status==STATUS_SUCCESS && us==20000u) {
        A04_PARENT_CONTEXT *c=context;uint16_t trim;
        if(a04_pmic_classd_trim(&c->pmic.bus,&trim))return -1;
        c->trimCode=trim&31u;c->trimCaptured=1;
        A04_TRACE("A04AUD MT6392 hardware trim code=%u (not AU_LOL ADC calibration)\n",(unsigned)c->trimCode);
    }
#endif
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
#if A04_SHARED_VOICE_DIAGNOSTIC || A04_SHARED_VOICE_PCM_DIAGNOSTIC
/* The retained first-playback provider uses the board Linux cold-start DC
 * profile and real MT6392 hardware trim. The older measurement-based path
 * still requires an AU_LOL-specific provider and does not reuse HP ADC data. */
static int ParentVoiceVerify(void *context,int16_t *initial,int16_t *value,unsigned *cap) {
    A04_PARENT_CONTEXT *c=context;
#if A04_SHARED_FIRST_PLAYBACK
    if(!c || !initial || !value || !cap || !c->audio.leased || c->audio.poisoned ||
       !c->trimCaptured || !c->digitalPlayback.prepared ||
       a04_mt6392_classd_verify_muted(&c->classd))return -1;
    /* Linux cold-start profile: kzalloc DC arrays until HP DC Offsets is
     * requested. No AU_LOL ADC channel or measured lineout calibration exists
     * in the board source. Do not relabel HP measurements as lineout data. */
    *initial=0;*value=0;*cap=1;return 0;
#else
    UNREFERENCED_PARAMETER(initial);UNREFERENCED_PARAMETER(value);UNREFERENCED_PARAMETER(cap);
    /* ParentLock serializes the WACS read with gain changes. This checks the
     * PMIC gain only; a board-route mute and voice calibration provider are
     * still absent and cannot be inferred from this register. */
    if(!c || !c->pmic.online || a04_mt6392_classd_verify_muted(&c->classd)) {
        A04_TRACE("A04AUD voice calibration rejected: classd mute unavailable or unverified\n");
        return -1;
    }
    A04_TRACE("A04AUD voice calibration rejected: AU_LOL calibration and downstream mute unverified\n");
    return -1;
#endif
}
static int ParentVoiceAfeRead(void *context,unsigned offset,uint32_t *value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->audioAfe || !value || (offset!=0xf00 && offset!=0xf0c && offset!=0xf2c))return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->audioAfe+offset));return 0;
}
static int ParentVoiceAfeWrite(void *context,unsigned offset,uint32_t value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->audioAfe || (offset!=0xf00 && offset!=0xf0c && offset!=0xf2c))return -1;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->audioAfe+offset),value);return 0;
}
static int ParentVoiceDelay(void *context,unsigned us) {
    LARGE_INTEGER interval;
    if(!context || (us!=400 && us!=2000) || KeGetCurrentIrql()>APC_LEVEL)return -1;
    interval.QuadPart=-(LONGLONG)us*10;
    return KeDelayExecutionThread(KernelMode,FALSE,&interval)==STATUS_SUCCESS?0:-1;
}
static struct a04_codec_voice_io ParentVoiceIo(A04_PARENT_CONTEXT *c) {
    struct a04_codec_voice_io io={{c,ParentDomainRead,ParentDomainWrite},
        {c,ParentVoiceAfeRead,ParentVoiceAfeWrite,ParentVoiceDelay},ParentVoiceVerify};
    return io;
}
#endif
static int ParentCodecStart(void *context) {
    A04_PARENT_CONTEXT *c=context;struct a04_mt6392_audio_rails rails;
#if A04_SHARED_VOICE_DIAGNOSTIC
    { int16_t initial,value;unsigned cap;
      if(ParentVoiceVerify(c,&initial,&value,&cap))return -1; }
#endif
#if A04_SHARED_FIRST_PLAYBACK
    if(!c || !c->pmic.online || a04_pmic_audio_power_on_pinned(&c->pmic.bus))return -1;
#endif
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
#if A04_SHARED_FIRST_PLAYBACK
    { struct a04_codec_domain_io io={c,ParentDomainRead,ParentDomainWrite};
      uint32_t con2;
      /* Board mt8167_codec_init_regs: LOUT gain9 = 0 dB, quasi-current0. */
      if(ParentDomainRead(c,0x708,&con2) ||
         a04_codec_domain_update(&io,0x708,(15u<<9)|(3u<<30),con2&((15u<<9)|(3u<<30)),9u<<9))return -1; }
#endif
    return a04_mt6392_classd_start_muted(&c->classd);
}
#if defined(A04_MT6392_UNMUTE_DIAGNOSTIC) && A04_MT6392_UNMUTE_DIAGNOSTIC
static int ParentCodecUnmute(void *context) {
    A04_PARENT_CONTEXT *c=context;
#if A04_SHARED_VOICE_DIAGNOSTIC
    struct a04_codec_voice_io io=ParentVoiceIo(c);
    /* Child calls unmute only after starting its diagnostic DMA; Class D is muted. */
    if(c->voice.poisoned || (!c->voice.active && a04_codec_voice_start(&c->voice,&io)))return -1;
#endif
    return a04_mt6392_classd_unmute_min(&c->classd);
}
#endif
static int ParentCodecMute(void *context) { return a04_mt6392_classd_mute(&((A04_PARENT_CONTEXT *)context)->classd); }
static int ParentCodecStop(void *context) {
    A04_PARENT_CONTEXT *c=context;
    if(!c)return -1;
#if A04_SHARED_VOICE_DIAGNOSTIC
    if(c->voice.active) {
        struct a04_codec_voice_io io=ParentVoiceIo(c);
        if(a04_codec_voice_stop(&c->voice,&io))return -1;
    }
#endif
    if(a04_mt6392_classd_stop(&c->classd))return -1;
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
#if A04_SHARED_DIGITAL_PLAYBACK
static int ParentDigitalRead(void *context,unsigned off,uint32_t *value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->audioAfe || !value || (off&3u) || off>=0x1000)return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->audioAfe+off));return 0;
}
static int ParentDigitalWrite(void *context,unsigned off,uint32_t value) {
    A04_PARENT_CONTEXT *c=context;
    if(!c || !c->audioAfe)return -1;
    switch(off) {
    case 0: case 0x10: case 0x24: case 0x28: case 0x34: case 0x6c:
    case 0x108: case 0x10c: case 0x124: case 0x260: case 0x264:
    case 0xf04: case 0xf0c: case 0xf10: case 0xf28: case 0xf2c: break;
    default:return -1;
    }
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->audioAfe+off),value);return 0;
}
static int ParentDigitalDelay(void *context,unsigned us) {
    if(!context || us!=100 || KeGetCurrentIrql()>APC_LEVEL)return -1;
    KeStallExecutionProcessor(us);return 0;
}
static int ParentPreparePlayback(PVOID context,ULONG rate) {
    A04_PARENT_CONTEXT *c;int result=-1;struct a04_digital_io io;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(c->audio.online && c->audio.leased && !c->audio.poisoned &&
       c->pmic.online && c->audioAfe && c->afeClockHeld && !c->afeClockPoisoned &&
       c->audioTopClock.held && !c->audioTopClock.poisoned &&
       c->codecDomain.held && !c->codecDomain.poisoned &&
       !a04_mt6392_classd_verify_muted(&c->classd)) {
        io.context=c;io.read=ParentDigitalRead;io.write=ParentDigitalWrite;io.delay_us=ParentDigitalDelay;
        result=a04_digital_prepare(&c->digitalPlayback,&io,rate);
    }
    if(c->digitalPlayback.poisoned || c->classd.poisoned || c->pmic.bus.poisoned) {
        c->audio.poisoned=1;result=-2;
    }
    ParentUnlock(c);return result;
}
#endif
#if A04_SHARED_FIRST_PLAYBACK
/* Selected by the board-specific first-playback build. Windows owns the
 * shared parent/PnP child ranges; all operations hold its single WACS lock.
 * Readiness tests observe hardware, and do not claim secure-world handoff. */
static int ParentReadAudioDiagnostic(PVOID context,struct a04_codec_snapshot *out) {
    A04_PARENT_CONTEXT *c;uint32_t value;uint16_t trim,oc,clock;
    struct a04_mt6392_audio_raw speaker;struct a04_mt6392_audio_rails rails;unsigned i;
    if(!out)return -1;
    RtlZeroMemory(out,sizeof(*out));
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    out->valid=A04_CODEC_VALID_PARENT;out->online=c->audio.online;
    out->leased=c->audio.leased;out->poisoned=c->audio.poisoned;
    out->power_pinned=c->audioPowerPinned;out->trim_captured=c->trimCaptured;out->trim_code=c->trimCode;
    out->digital_prepared=c->digitalPlayback.prepared;out->voice_active=c->voice.active;
    if(c->audio.online && c->pmic.online && c->audioPowerPinned) {
        if(!ParentRegRead(c,A04_SHARED_TOP,0x24,&value)){out->top_clock=value;out->valid|=A04_CODEC_VALID_CLOCK;}
        for(i=0;i<5;i++)if(ParentDomainRead(c,0x700+i*4,&out->analog[i]))break;
        if(i==5)out->valid|=A04_CODEC_VALID_ANALOG;
        if(!a04_pmic_audio_observe(&c->pmic.bus,&speaker)) {
            out->spk0=speaker.spk_con0;out->spk9=speaker.spk_con9;out->spk11=speaker.spk_con11;
            out->valid|=A04_CODEC_VALID_SPEAKER;
        }
        if(!a04_pmic_audio_rails(&c->pmic.bus,&rails)) {
            out->vaud22=rails.vaud22;out->vaud22_selector=rails.vaud22_selector;out->vaud28=rails.vaud28;
            out->valid|=A04_CODEC_VALID_RAILS;
        }
        if(!a04_pmic_classd_trim(&c->pmic.bus,&trim) && !a04_pmic_classd_overcurrent(&c->pmic.bus,&oc) &&
           !a04_pmic_spk_clock_status(&c->pmic.bus,&clock)) {
            out->trim_raw=trim;out->overcurrent=oc;out->pmic_clock=clock;out->valid|=A04_CODEC_VALID_TRIM_OC;
        }
        if(c->audioAfe && c->afeClockHeld && !c->afeClockPoisoned) {
            ParentDigitalRead(c,0x10,&out->afe_dac0);ParentDigitalRead(c,0,&out->afe_top);
            ParentDigitalRead(c,0x44,&out->afe_cur);ParentDigitalRead(c,0x3a4,&out->afe_irq);
            out->valid|=A04_CODEC_VALID_AFE;
        }
        if(c->pmic.bus.poisoned)c->audio.poisoned=1;
        out->poisoned=c->audio.poisoned;
    }
    ParentUnlock(c);return 0;
}
static int ParentFirstPreflight(void *context,uint32_t rate) {
    A04_PARENT_CONTEXT *c=context;struct a04_mt6392_audio_raw spk;
    uint32_t con1,con2,dac,abb;
    if(!c || rate!=48000 || !c->pmic.online || c->pmic.bus.poisoned ||
       !c->topck || !c->apmixed || c->apmixedLength<0x714 || !c->audioAfe ||
       c->afeClockHeld || c->afeClockPoisoned || c->audioTopClock.held || c->audioTopClock.poisoned ||
       c->codecDomain.held || c->codecDomain.poisoned || c->digitalPlayback.retained ||
       c->classd.active || c->classd.poisoned || c->speakerClock.held ||
       ParentDomainRead(c,0x704,&con1) || (con1&((1u<<15)|(1u<<14)|(1u<<12)|(1u<<11))) ||
       ParentDomainRead(c,0x708,&con2) || (con2&(1u<<8)) ||
       ParentDigitalRead(c,0x10,&dac) || (dac&0xdeu) ||
       ParentDigitalRead(c,0xf00,&abb) || (abb&3u) ||
       a04_pmic_audio_observe(&c->pmic.bus,&spk) || (spk.spk_con0&1u))return -1;
    return 0;
}
/* Deliberately no release/drain certificate for a one-shot retained lease. */
static int ParentPinnedRelease(void *context) {UNREFERENCED_PARAMETER(context);return -1;}
static int ParentFirstCodecReady(void *context) {
    A04_PARENT_CONTEXT *c=context;struct a04_mt6392_audio_rails rails;
    struct a04_mt6392_audio_raw spk;uint32_t con1,con2;
    if(!c || !c->pmic.online || c->pmic.bus.poisoned ||
       !c->audioTopClock.held || c->audioTopClock.poisoned || !c->afeClockHeld || c->afeClockPoisoned ||
       ParentDomainRead(c,0x704,&con1) || (con1&(1u<<15)) ||
       ParentDomainRead(c,0x708,&con2) || (con2&(1u<<8)) ||
       a04_pmic_audio_rails(&c->pmic.bus,&rails) || !a04_pmic_audio_rails_ready(&rails) ||
       a04_pmic_audio_observe(&c->pmic.bus,&spk) || (spk.spk_con0&1u))return -1;
    return 0;
}
static int ParentFirstUnmuteReady(void *context) {
    A04_PARENT_CONTEXT *c=context;uint32_t dac;uint16_t oc;
    struct a04_mt6392_audio_rails rails;
    if(!c || !c->audio.leased || !c->audio.online || c->audio.poisoned ||
       !c->trimCaptured || !c->voice.active || c->voice.poisoned ||
       !c->digitalPlayback.prepared || c->digitalPlayback.poisoned ||
       ParentDigitalRead(c,0x10,&dac) || (dac&3u)!=3u ||
       a04_pmic_audio_rails(&c->pmic.bus,&rails) || !a04_pmic_audio_rails_ready(&rails) ||
       a04_pmic_classd_overcurrent(&c->pmic.bus,&oc) || (oc&(1u<<14)))return -1;
    return 0;
}
#endif
#if A04_SHARED_CAPTURE
#include "capture_parent.inc"
#endif
static int ParentAudioAcquire(PVOID context,ULONG rate) {
    A04_PARENT_CONTEXT *c;int result;
#if A04_SHARED_FIRST_PLAYBACK
    int keepPower=0;
#endif
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
#if A04_SHARED_FIRST_PLAYBACK
    if(!NT_SUCCESS(WdfDeviceStopIdle((WDFDEVICE)context,TRUE)))return -1;
#endif
    c=ParentContext((WDFDEVICE)context);
    if(ParentLock(c)) {
#if A04_SHARED_FIRST_PLAYBACK
        WdfDeviceResumeIdle((WDFDEVICE)context);
#endif
        return -1;
    }
#if A04_SHARED_CAPTURE
    if(c->captureInput.reserved){ParentUnlock(c);WdfDeviceResumeIdle((WDFDEVICE)context);return -1;}
#endif
    result=a04_audio_owner_acquire(&c->audio,rate);
#if A04_SHARED_FIRST_PLAYBACK
    if((!result || c->audio.poisoned) && !c->audioPowerPinned) {c->audioPowerPinned=1;keepPower=1;}
#endif
    A04_TRACE("A04AUD parent acquire rate=%lu result=%d online=%d leased=%d poisoned=%d\n",rate,result,c->audio.online,c->audio.leased,c->audio.poisoned);
    if(result && c->audio.poisoned)result=-2;
    ParentUnlock(c);
#if A04_SHARED_FIRST_PLAYBACK
    if(!keepPower)WdfDeviceResumeIdle((WDFDEVICE)context);
#endif
    return result;
}
static int ParentAudioUnmute(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()>APC_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
#if A04_SHARED_VOICE_PCM_DIAGNOSTIC
    if(!c->voice.active || c->voice.poisoned){ParentUnlock(c);return -1;}
#endif
    result=a04_audio_owner_unmute(&c->audio);ParentUnlock(c);return result;
}
static int ParentAudioMute(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()>APC_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    result=a04_audio_owner_mute(&c->audio);ParentUnlock(c);return result;
}
#if A04_SHARED_VOICE_PCM_DIAGNOSTIC
static int ParentVoiceOperation(PVOID context,int start) {
    A04_PARENT_CONTEXT *c;int result=-1;
    struct a04_codec_voice_io io;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(c->audio.online && c->audio.leased && !c->audio.poisoned && c->codecDomain.held &&
       !c->codecDomain.poisoned && c->audioAfe) {
        io=ParentVoiceIo(c);
        result=start?a04_codec_voice_start(&c->voice,&io):a04_codec_voice_stop(&c->voice,&io);
        if(c->voice.poisoned || c->classd.poisoned || c->pmic.bus.poisoned)c->audio.poisoned=1;
    }
    ParentUnlock(c);return result;
}
static int ParentStartVoice(PVOID context) { return ParentVoiceOperation(context,1); }
static int ParentStopVoice(PVOID context) { return ParentVoiceOperation(context,0); }
static int ParentVerifyDmaDrained(PVOID context) {
    UNREFERENCED_PARAMETER(context);
    /* DL1 disable/IRQ readback alone does not prove fabric or DMA drain. */
    A04_TRACE("A04AUD DMA drain rejected: outstanding fabric transactions unverified\n");
    return -1;
}
static int ParentVerifyLivePcm(PVOID context,PVOID mdl,uint64_t logical,ULONG size) {
#if defined(A04_SHARED_ENGINE_PCM) && A04_SHARED_ENGINE_PCM
    A04_PARENT_CONTEXT *c;PMDL pages=(PMDL)mdl;PPFN_NUMBER pfns;
    ULONG i,count;int result=-1;
    if(!context || !pages || !size || size>1048576 || (size&(PAGE_SIZE-1)) ||
       !logical || (logical&(PAGE_SIZE-1)) || logical>0x100000000ULL-size ||
       MmGetMdlByteOffset(pages) || MmGetMdlByteCount(pages)!=size ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    /* The matched child owns PortCls pages, validates kernel Normal-NC,
     * and requests MmWriteCombined for both aliases. Cache admission is the
     * child's responsibility; this callback verifies lease and DMA identity.
     * Reject HAL bounce/map-register copies: a
     * once-only flush cannot keep an engine-written cyclic buffer current. */
    pfns=MmGetMdlPfnArray(pages);count=size/PAGE_SIZE;
    for(i=0;i<count;++i)
        if(((uint64_t)pfns[i]<<PAGE_SHIFT)!=logical+(uint64_t)i*PAGE_SIZE)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(c->audio.leased && !c->audio.poisoned && c->afeClockHeld &&
       !c->afeClockPoisoned && c->codecDomain.held && !c->codecDomain.poisoned &&
       c->audioAfe)result=0;
    ParentUnlock(c);
    A04_TRACE("A04AUD engine PCM identity DMA admission=%d bytes=%lu; cache=child-noncached-contract drain=unknown\n",result,size);
    return result;
#else
    UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(mdl);
    UNREFERENCED_PARAMETER(logical);UNREFERENCED_PARAMETER(size);
    A04_TRACE("A04AUD live PCM rejected: engine PCM profile not selected\n");
    return -1;
#endif
}
#endif
static int ParentAudioRelease(PVOID context) {
    A04_PARENT_CONTEXT *c;int result;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
#if A04_SHARED_VOICE_PCM_DIAGNOSTIC
    if(c->voice.active || c->voice.poisoned){ParentUnlock(c);return -1;}
#endif
    #if A04_SHARED_DIGITAL_PLAYBACK
    if(c->digitalPlayback.retained){ParentUnlock(c);return -1;}
#endif
    result=a04_audio_owner_release(&c->audio);ParentUnlock(c);return result;
}
/* Only a fully acquired exclusive audio lease may mask and clear the IRQ.
 * The preflight contract must exclude HDMI/TDM and all other AFE users. */
static int ParentAudioPrepareIrq(PVOID context,PVOID afe,ULONG length) {
#if A04_SHARED_AUDIO_IRQ_PREP_DIAGNOSTIC
    A04_PARENT_CONTEXT *c;volatile UCHAR *base;ULONG dac,irq,irq2,status;int result=-1;
    if(!context || !afe || length!=0x1000 || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return -1;
    if(afe!=c->audioAfe || !c->audio.online ||
#if A04_SHARED_CAPTURE
       (!c->audio.leased && (!c->captureInput.ready || c->captureInput.poisoned)) ||
#else
       !c->audio.leased ||
#endif
       c->audio.poisoned ||
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
/* Opt-in real lease. Parent PnP owns TOP/GPIO/PWRAP; the single MSDC2
 * PDO owns the host/IRQ. This cold-start mode resets the whole combo, so no
 * Bluetooth/FM child may share it. Not a firmware handoff certificate. */
static NTSTATUS ParentWifiAcquire(PVOID context,ULONG *generation)
{
#if A04_SHARED_WIFI_RUNTIME
 A04_PARENT_CONTEXT *c;int e;
 if(!context||!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *generation=0;c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->wifiActive||c->wifiRtc.held||c->wifiRtc.uncertain||!c->pmic.online||c->pmic.bus.poisoned||
    !c->regs.online||c->regs.poisoned||!c->topck||!c->gpio||!c->pwrap){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 if(c->wifiGeneration==0xffffffffu){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
#if A04_SHARED_WIFI_RTC_BORROW
 e=a04_pmic_rtc32k_acquire_shared(&c->pmic.bus,&c->wifiRtc);
 A04_TRACE("A04WIFI RTC lease result=%d held=%d inherited=%d uncertain=%d\n",e,c->wifiRtc.held,c->wifiRtc.inherited,c->wifiRtc.uncertain);
#else
 e=a04_pmic_rtc32k_acquire(&c->pmic.bus,&c->wifiRtc);
#endif
 if(e&&!c->wifiRtc.held&&!c->wifiRtc.uncertain){ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 /* A partially changed RTC is a retained lease, never a clean rejection. */
 c->pmic.rtc_lease=&c->wifiRtc;c->wifiGeneration++;c->wifiActive=e?-1:1;
 *generation=c->wifiGeneration;ParentUnlock(c);return STATUS_SUCCESS;
#else
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(generation);return STATUS_DEVICE_NOT_READY;
#endif
}
static NTSTATUS ParentWifiRelease(PVOID context,ULONG generation)
{
#if A04_SHARED_WIFI_RUNTIME
 A04_PARENT_CONTEXT *c;uint32_t value,dir;int e;
 if(!context||!generation||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return STATUS_DEVICE_NOT_READY;
 if(c->wifiActive!=1||generation!=c->wifiGeneration||c->regs.poisoned||c->pmic.bus.poisoned||
    ParentRegRead(c,A04_SHARED_GPIO,0x100,&value)||ParentRegRead(c,A04_SHARED_GPIO,0,&dir)||
    (value&0x90u)||(dir&0x90u)!=0x90u){ParentUnlock(c);return STATUS_DEVICE_BUSY;}
 e=a04_pmic_rtc32k_release(&c->pmic.bus,&c->wifiRtc);
 if(e){c->wifiActive=-1;ParentUnlock(c);return STATUS_DEVICE_NOT_READY;}
 c->pmic.rtc_lease=NULL;c->wifiActive=0;ParentUnlock(c);return STATUS_SUCCESS;
#else
 UNREFERENCED_PARAMETER(context);UNREFERENCED_PARAMETER(generation);return STATUS_DEVICE_NOT_READY;
#endif
}
static int ParentWifiLive(PVOID context,ULONG generation)
{
 A04_PARENT_CONTEXT *c;int ok;
 if(!context||KeGetCurrentIrql()!=PASSIVE_LEVEL)return 0;
 c=ParentContext((WDFDEVICE)context);if(ParentLock(c))return 0;
 ok=c->wifiActive==1 && c->regs.online && !c->regs.poisoned && generation && generation==c->wifiGeneration;
 ExReleaseFastMutex(&c->pwrapLock);return ok;
}
static int ParentWifiUpdateAllowed(unsigned bank,unsigned off,ULONG clear,ULONG set)
{
 unsigned p,shift;
 if(bank==0)return off==0x40 && clear==(7u<<3) && set==0;
 if(bank!=1)return 0;
 for(p=0;p<8;p++){
  static const unsigned pins[]={4,7,68,69,70,71,72,73};
  unsigned pin=pins[p];shift=(pin%5)*3;
  if(off==0x300+(pin/5)*16 && clear==(7u<<shift) && set==((pin>=68?1u:0u)<<shift))return 1;
 }
 return 0;
}
static int ParentWifiWriteAllowed(unsigned b,unsigned off,ULONG v)
{
 if(b==0)return (off==0xa0&&v==2)||(off==0x9c&&v==1)||(off==0x6c&&v==(1u<<30))||(off==0xb0&&v==2);
 if(b!=1)return 0;
 return ((off==0x104||off==0x108||off==4)&&(v==(1u<<4)||v==(1u<<7))) ||
  (off==0xe48&&v==((1u<<6)|(1u<<5)|(1u<<10)|(1u<<9)|(1u<<14)|(1u<<13))) ||
  (off==0xe44&&v==((1u<<4)|(1u<<8)|(1u<<12))) ||
  (off==0xe58&&v==((1u<<10)|(1u<<9)|(1u<<2)|(1u<<1)|(1u<<4))) ||
  (off==0xe54&&v==((1u<<8)|1u|(1u<<6)|(1u<<5))) || (off==0x934&&v==0x7e);
}
static int ParentWifiOperation(PVOID context,ULONG generation,unsigned bank,unsigned off,ULONG clear,ULONG value,int update)
{
 A04_PARENT_CONTEXT *c;enum a04_shared_bank b;uint32_t old;int result=-1;
 if(!context||KeGetCurrentIrql()!=PASSIVE_LEVEL || (update?!ParentWifiUpdateAllowed(bank,off,clear,value):!ParentWifiWriteAllowed(bank,off,value)))return -1;
 c=ParentContext((WDFDEVICE)context);b=bank==0?A04_SHARED_TOP:A04_SHARED_GPIO;
 if(ParentLock(c))return -1;
 if(c->wifiActive!=1||!generation||generation!=c->wifiGeneration||!c->regs.online||c->regs.poisoned)goto done;
 if(update){if(ParentRegRead(c,b,off,&old)||old==UINT32_MAX)goto fault;value=(old&~clear)|value;}
 if(ParentRegWrite(c,b,off,value))goto fault;
 result=0;goto done;
fault:c->regs.poisoned=1;c->wifiActive=0;
done:ExReleaseFastMutex(&c->pwrapLock);return result;
}
static int ParentWifiUpdate(PVOID c,ULONG g,unsigned b,unsigned o,ULONG clear,ULONG set)
{return ParentWifiOperation(c,g,b,o,clear,set,1);}
static int ParentWifiWrite(PVOID c,ULONG g,unsigned b,unsigned o,ULONG v)
{return ParentWifiOperation(c,g,b,o,0,v,0);}
static VOID ParentInterfaceReference(PVOID context) { WdfObjectReference((WDFDEVICE)context); }
static VOID ParentInterfaceDereference(PVOID context) { WdfObjectDereference((WDFDEVICE)context); }
#if A04_SHARED_GPU_PLATFORM
#include "gpu_parent_service.inc"
#endif
#if A04_SHARED_SD_POWER
#include "../../storage/mt8167-sd/power_parent.inc"
#endif
#if MT6392_RTC_CALENDAR
static NTSTATUS Mt6392ReadCalendar(PVOID context,struct mt6392_rtc_raw *out)
{
    int result;
    if(!context || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    result=mt6392_pwrap_read_calendar(&ParentContext((WDFDEVICE)context)->pmic,out);
    if(result==MT6392_RTC_UNSTABLE)return STATUS_RETRY;
    return result==0?STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;
}
#endif
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
 A04_PARENT_CONTEXT*c=p;
 if(!c->gpio)return -1;
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 /* Board usb20_host.c has CONFIG_OC disabled. Do not make an unconfigured
  * GPIO21 a mandatory overcurrent measurement. This profile has ID/source
  * health only, not overcurrent protection. ID41 uses mux 2 (IDDIG). */
 return (ParentUsbGpioRead(c,0x20)&(1u<<9)) ||
  (ParentUsbGpioRead(c,0x220)&(1u<<9)) ||
  (ParentUsbGpioRead(c,0x380)&(7u<<3))!=(2u<<3) ? -1:0;
#else
 unsigned mode=ParentUsbGpioRead(c,0x340),dir=ParentUsbGpioRead(c,0x10),input=ParentUsbGpioRead(c,0x210);
 return (mode&(7u<<3)) || (dir&(1u<<5)) || !(input&(1u<<5)) ? -1:0;
#endif
}
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
/* a04br3 Linux switches GPIO22 and its BQ24160 configuration has no action
 * in tbl_charger_otg_vbus. This is a board-specific, unverified assumption:
 * GPIO22 itself controls the source. Do not claim a BQ boost was enabled.
 * Called under the parent mutex, before GPIO22 is raised. */
static int ParentUsbProbeSource(void *p)
{
 A04_PARENT_CONTEXT*c=p;unsigned input;
 if(!c->gpio)return -1;
 input=ParentUsbGpioRead(c,0x220);
 /* PC cable: ID41 is high or external VBUS detect34 is low. Decline before
  * any pin, clock, rail or PHY change. This is eligibility, not a fault. */
 return A04UsbAdmission(input);
}
static int ParentUsbSourceAcquire(void *p)
{
 A04_PARENT_CONTEXT*c=p; unsigned dir,in,mode;
 if(!c->gpio)return -1;
 dir=ParentUsbGpioRead(c,0x20);in=ParentUsbGpioRead(c,0x220);mode=ParentUsbGpioRead(c,0x360);
 /* ID41 grounded and USB-detect34 inactive before sourcing; do not drive
  * into a detected external source. The latter is active low in Linux. */
 if((ParentUsbGpioRead(c,0x380)&(7u<<3))!=(2u<<3) || (dir&((1u<<9)|(1u<<2))) || (in&(1u<<9)) || !(in&(1u<<2)) || (mode&(7u<<12)))return -1;
 return 0;
}
static int ParentUsbSourceRelease(void *p) { UNREFERENCED_PARAMETER(p);return 0; }
#endif
static int ParentUsbVerify(void *p) {
 A04_PARENT_CONTEXT*c=p; ULONG sel,con,pwr,rate;
 if(!c->topck || !c->apmixed)return -1;
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 if(!c->gpio || A04UsbPinsPrepare(&c->usb.gpio))return -1;
#endif
 sel=ParentUsbTopRead(c,4); (void)sel; /* Used by the optional mux switch below. */ con=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x140));
 pwr=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x150));
 rate=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x144));
 /* Verify the shared PLL; never retune it on behalf of USB. */
 if((con&0x30000001u)!=0x30000001u || (pwr&3)!=1 || (rate&0x7fu)!=48 ||
    ((rate>>24)&7u)!=0 || rate!=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->apmixed+0x144)))return -1;
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
 if(((sel>>20)&7u)!=2u){
  /* clk-mt8167.c: USB78 mux at 4[22:20], parent2=UNIVPLL/16.
   * Change only with both its USB20 and USB11 consumers gated. */
  if(!(ParentUsbTopRead(c,0x24)&(1u<<13)) || !(ParentUsbTopRead(c,0x3c)&(1u<<14)))return -1;
  ParentUsbTopWrite(c,4,(sel&~(7u<<20))|(2u<<20));
 }
#endif
 return ((ParentUsbTopRead(c,4)>>20)&7u)==2u ? 0:-1;
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
static NTSTATUS ParentThermalVerifyHeadphone(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(a04_shared_thermal_valid(&c->thermal,generation) && c->thermal.inflight) {
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC && A04_AUDIO_DIAGNOSTIC_TRACE
  /* The active codec lease owns APMIXED and PWRAP while the shared mutex
   * is held. These reads are observations only, not proof of board mute. */
  if(c->audio.leased && !c->audio.poisoned && c->codecDomain.held &&
     !c->codecDomain.poisoned && c->classd.active && !c->classd.poisoned && c->pmic.online) {
   uint32_t con2;int readResult=ParentDomainRead(c,0x708,&con2);
   int muteResult=a04_mt6392_classd_verify_muted(&c->classd);
   if(c->classd.poisoned || c->pmic.bus.poisoned)c->audio.poisoned=1;
   A04_TRACE("A04ADC codec observation route-read=%d hp-left-adc=%d gain-muted=%d; physical mute unknown\n",
             readResult,readResult?0:!!(con2&(1u<<15)),muteResult==0);
  }
#endif
  /* No provider establishes both route and physical downstream mute yet. */
  result=STATUS_DEVICE_NOT_READY;
 }
 ParentUnlock(c);return result;
}
static NTSTATUS ParentThermalVerifyOwnership(PVOID p,ULONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(ParentLock(c))return result;
 if(a04_shared_thermal_ownership_verified(&c->thermal,generation))
  result=STATUS_SUCCESS;
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
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
#include "thermal_native_parent.inc"
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
#if A04_SHARED_SD_POWER
    if(c->sdPower.held)c->sdPower.poisoned=1;
#endif
#if A04_SHARED_FIRST_PLAYBACK
    if(can_access && !c->removed && c->classd.active && c->pmic.online)
        (void)a04_mt6392_classd_mute(&c->classd);
#endif
#if A04_SHARED_GPU_PLATFORM
    a04_gpu_domain_revoke(&c->gpuDomain);
#endif
    c->wifiActive=0;
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
#if A04_SHARED_CAPTURE
    if(c->captureInput.reserved||c->captureInput.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_GPU_PLATFORM
    if(c->gpuDomain.held || c->gpuDomain.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_SD_POWER
    if(c->sdPower.held || c->sdPower.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if defined(A04_MT6392_RTC32K_DIAGNOSTIC) && A04_MT6392_RTC32K_DIAGNOSTIC
    /* A retained or ambiguous RTC lease must outlive the PWRAP mapping. */
    if(c->pmic.rtc_lease)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(c->thermal.leased || c->thermal.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_MT8167_CODEC_DOMAIN_DIAGNOSTIC
    if(c->codecDomain.held || c->codecDomain.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_AUDIO_AFE_CLOCK_DIAGNOSTIC
    if(c->afeClockHeld || c->afeClockPoisoned)return STATUS_DEVICE_BUSY;
#if A04_SHARED_DIGITAL_PLAYBACK
    if(c->digitalPlayback.retained)return STATUS_DEVICE_BUSY;
#endif
#endif
#if A04_SHARED_AUDIO_TOP_GATE_DIAGNOSTIC
    if(c->audioTopClock.held || c->audioTopClock.poisoned)return STATUS_DEVICE_BUSY;
#endif
#if A04_MT6392_CLK_ALIAS_EXPERIMENT
    if(c->speakerClock.held)return STATUS_DEVICE_BUSY;
#endif
#if A04_SHARED_GPU_DOMAIN_SPM
    if(c->gpuSpm){MmUnmapIoSpace(c->gpuSpm,0x1000);c->gpuSpm=NULL;}
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
    A04_PARENT_CONTEXT *c=ParentContext(dev);struct a04_parent_resource r[8],t[8];
    struct a04_parent_mapping mapping;ULONG i,n=0;PHYSICAL_ADDRESS pa;
#if A04_SHARED_APMIXED_RESOURCE
    uint64_t apmixed=0;
#if A04_SHARED_GPU_DOMAIN_SPM
    uint64_t gpuSpm=0;
#endif
#endif
#if A04_SHARED_GPU_DOMAIN_SPM
    if(c->gpuSpm)return STATUS_INVALID_DEVICE_STATE;
#endif
    if(c->topck||c->infra||c->gpio||c->pwrap||c->apmixed)return STATUS_INVALID_DEVICE_STATE;
    if(!raw || !translated || WdfCmResourceListGetCount(raw)!=WdfCmResourceListGetCount(translated))return STATUS_DEVICE_CONFIGURATION_ERROR;
    for(i=0;i<WdfCmResourceListGetCount(translated);++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR original=WdfCmResourceListGetDescriptor(raw,i);
        if(!d || !original || d->Type!=original->Type)return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->Type!=CmResourceTypeMemory)continue;
#if A04_SHARED_GPU_THERMAL_PROFILE
        if((d->Flags|original->Flags)&CM_RESOURCE_MEMORY_READ_ONLY)return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
        if(n>=(A04_SHARED_GPU_THERMAL_PROFILE?8u:(A04_SHARED_GPU_DOMAIN_SPM?6u:5u)))return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->u.Memory.Start.QuadPart<0 || original->u.Memory.Start.QuadPart<0)return STATUS_DEVICE_CONFIGURATION_ERROR;
        r[n].base=(uint64_t)original->u.Memory.Start.QuadPart;r[n].length=original->u.Memory.Length;
        r[n].exclusive=original->ShareDisposition==CmResourceShareDeviceExclusive ? 1u:0u;
        t[n].base=(uint64_t)d->u.Memory.Start.QuadPart;t[n].length=d->u.Memory.Length;
        t[n].exclusive=d->ShareDisposition==CmResourceShareDeviceExclusive ? 1u:0u;++n;
    }
#if A04_SHARED_APMIXED_RESOURCE
#if A04_SHARED_GPU_DOMAIN_SPM
#if A04_SHARED_GPU_THERMAL_PROFILE
    if(a04_parent_translate_gpu_thermal_resources(r,t,n,&mapping,&apmixed,&gpuSpm))return STATUS_DEVICE_CONFIGURATION_ERROR;
    c->thermalAssigned[0].QuadPart=c->thermalAssigned[1].QuadPart=0;
    for(i=0;i<n;++i){
        if(r[i].base==0x1100d000u)c->thermalAssigned[0].QuadPart=(LONGLONG)t[i].base;
        if(r[i].base==0x11003000u)c->thermalAssigned[1].QuadPart=(LONGLONG)t[i].base;
    }
    if(!c->thermalAssigned[0].QuadPart||!c->thermalAssigned[1].QuadPart)return STATUS_DEVICE_CONFIGURATION_ERROR;
#else
    if(a04_parent_translate_gpu_domain_resources(r,t,n,&mapping,&apmixed,&gpuSpm))return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
#else
    /* Integrated parent keeps the platform's existing four/five windows.
     * Absence of APMIXED only withholds USB/audio admission, not input D0. */
    if(!A04_SHARED_USB_ONLY && n==4){
        if(a04_parent_translate_resources(r,t,n,&mapping))return STATUS_DEVICE_CONFIGURATION_ERROR;
    } else if(a04_parent_translate_audio_resources(r,t,n,&mapping,&apmixed))return STATUS_DEVICE_CONFIGURATION_ERROR;
#endif
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
    if(apmixed){
        for(i=0;i<n;++i)if(r[i].base==0x10018000u)break;
        if(i==n)goto fail;
        c->apmixedLength=t[i].length;
        pa.QuadPart=(LONGLONG)apmixed;c->apmixed=MmMapIoSpace(pa,c->apmixedLength,MmNonCached);
        if(!c->apmixed)goto fail;
    }
#endif
#if A04_SHARED_GPU_DOMAIN_SPM
    pa.QuadPart=(LONGLONG)gpuSpm;c->gpuSpm=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->gpuSpm)goto fail;
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
    UNREFERENCED_PARAMETER(target);
#if A04_SHARED_SD_POWER
    /* PowerRelations must put the consumer through quiesce/release first.
     * An unresolved lease is a real failure, never an orderly power-off. */
    {
     A04_PARENT_CONTEXT*c=ParentContext(dev);int held;
     InterlockedExchange(&c->admissionClosed,1);ExAcquireFastMutex(&c->pwrapLock);
     held=c->sdPower.held;ExReleaseFastMutex(&c->pwrapLock);
     if(held)return STATUS_DEVICE_BUSY;
    }
#endif
    ParentStop(ParentContext(dev),1);return STATUS_SUCCESS;
}
NTSTATUS ParentD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
    UNREFERENCED_PARAMETER(previous);
#if A04_SHARED_BUTTON_D0_EXPERIMENT
    A04_PARENT_CONTEXT *c=ParentContext(dev);NTSTATUS result=STATUS_DEVICE_NOT_READY;ULONG init,state;
#if A04_SHARED_THERMAL_DIAGNOSTIC && A04_AUDIO_DIAGNOSTIC_TRACE
    struct a04_adc_observation observation;ULONG observationBytes=sizeof(observation),observationAttributes=0;
    UNICODE_STRING observationName=RTL_CONSTANT_STRING(L"A04BR3AdcObservation");
    GUID observationGuid={0x91b27b25,0x4cc5,0x4471,{0xa0,0x4b,0x81,0x67,0x03,0x01,0x00,0x01}};
    NTSTATUS observationStatus;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
    if(c->thermalRundownClosed){ExReInitializeRundownProtection(&c->thermalRundown);c->thermalRundownClosed=FALSE;}
#endif
    /* 1=closed, 2=starting, 0=open. Stop can cancel a start before the
     * final compare-exchange; surprise removal prevents any later restart. */
    if (InterlockedCompareExchange(&c->removed,0,0) ||
        InterlockedCompareExchange(&c->admissionClosed,2,1)!=1) return result;
#if A04_SHARED_THERMAL_DIAGNOSTIC && A04_AUDIO_DIAGNOSTIC_TRACE
    RtlZeroMemory(&observation,sizeof(observation));
    observationStatus=ExGetFirmwareEnvironmentVariable(&observationName,&observationGuid,&observation,&observationBytes,&observationAttributes);
    if(NT_SUCCESS(observationStatus) && a04_adc_observation_valid(&observation,observationBytes,observationAttributes))
        A04_TRACE("A04ADC early DXE clock=%08lx; EBS and secure release unknown\n",(ULONG)observation.clock_first);
    else A04_TRACE("A04ADC early DXE observation unavailable or invalid\n");
#endif
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
#if A04_SHARED_GPU_PLATFORM
    if(!c->gpuDomain.held && !c->gpuDomain.poisoned)c->gpuDomain.revoked=0;
#endif
    c->pmic.online=1;
#if A04_SHARED_FIRST_PLAYBACK
    c->audio.online=(c->apmixed && c->apmixedLength>=0x714 && !c->audio.poisoned && !c->audio.leased && !c->audioPowerPinned);
    c->regs.online=c->regs.poisoned?0:1;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
    /* Legacy generic ADC admission remains closed. Native Measure only opens
     * its own lease after the real board record + live mode/reset check. */
    c->thermal.online=1;c->thermal.firmware_released=0;c->thermal.adc_exclusive=0;
#endif
    c->usb.online=(c->apmixed && !c->usb.poisoned) ? 1 : 0;
#if A04_SHARED_TOUCH_D0_EXPERIMENT
    c->regs.online=c->regs.poisoned ? 0 : 1;
#endif
    if (InterlockedCompareExchange(&c->admissionClosed,0,2)==2) result=STATUS_SUCCESS;
    else { c->pmic.online=0; c->usb.online=0; c->regs.online=0;c->audio.online=0; }
done:
    if (!NT_SUCCESS(result)) { c->pmic.online=0; c->usb.online=0; c->regs.online=0;c->audio.online=0;(void)InterlockedCompareExchange(&c->admissionClosed,1,2); }
    A04_TRACE("A04AUD parent d0 result=%08lx poisoned=%d audio-online=%d\n",(ULONG)result,c->pmic.bus.poisoned,c->audio.online);
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
#if MT6392_RTC_CALENDAR
    /* Register access stays in the parent: this child claims no MMIO/IRQ. */
    if(c->base==0xfffffffdu)return STATUS_SUCCESS;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
    /* These ranges are already assigned to the eight-range parent. A child
     * must not submit a second exclusive claim to the system arbiter. */
    if(c->base==0xfffffffeu)return STATUS_SUCCESS;
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
    if(c->base!=0x10002000u && c->base!=0x11009000u && c->base!=0x11100000u && c->base!=0x11170000u)return STATUS_DEVICE_CONFIGURATION_ERROR;
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
#if A04_SHARED_WIFI_IRQ_DIAGNOSTIC
    if(c->base==0x11170000u) {
        /* a04br3 DTS: GIC SPI 109 => GSIV 141, level low. Requirements
         * do not encode polarity: platform routing must supply it. This is
         * MSDC2's interrupt, not the separate combo GPIO/EINT5. */
        RtlZeroMemory(&descriptor,sizeof(descriptor));
        descriptor.Type=CmResourceTypeInterrupt;
        descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
        descriptor.Flags=CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE;
        descriptor.u.Interrupt.MinimumVector=141;
        descriptor.u.Interrupt.MaximumVector=141;
        status=WdfIoResourceListAppendDescriptor(list,&descriptor);
        if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
    }
#endif
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
#if MT6392_RTC_CALENDAR
    if(base==0xfffffffdu) {
        WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&Mt6392RtcInterfaceGuid,NULL);
        query.SendQueryToParentStack=TRUE;
        status=WdfDeviceAddQueryInterface(child,&query);
        if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    }
#endif
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
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
    if(base==0xfffffffeu) {
        WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&Mt8167ThermalParentPioGuid,NULL);
        query.SendQueryToParentStack=TRUE;status=WdfDeviceAddQueryInterface(child,&query);
        if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    }
#endif
    if(base==0xffffffffu) {
        /* AFE is a synthesized child, not an ACPI PDO. Preserve the real
         * parent-bus DMA/translation interface, including its ref callbacks;
         * never return fabricated bus methods or an identity-translation stub.
         * WDF_QUERY_INTERFACE_CONFIG.SendQueryToParentStack is the documented
         * PDO forwarding path. Existing touch/USB/WiFi interfaces are unchanged. */
#if A04_SHARED_CAPTURE
        WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04CaptureInterfaceGuid,NULL);
        query.SendQueryToParentStack=TRUE;
        status=WdfDeviceAddQueryInterface(child,&query);
        if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
#endif
        WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&GUID_BUS_INTERFACE_STANDARD,NULL);
        query.SendQueryToParentStack=TRUE;
        status=WdfDeviceAddQueryInterface(child,&query);
        if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    }
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedAudioInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;
    status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    if(base==0x11170000u) {
        WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedWifiInterfaceGuid,NULL);
        query.SendQueryToParentStack=TRUE;
        status=WdfDeviceAddQueryInterface(child,&query);
        if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    }
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
#if MT6392_RTC_CALENDAR
    MT6392_RTC_INTERFACE rtcIface;
#endif
#if A04_SHARED_THERMAL_DIAGNOSTIC
    A04_SHARED_THERMAL_INTERFACE thermalIface;
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
    MT8167_THERMAL_PARENT_PIO_INTERFACE nativeIface;
#endif
#endif
    A04_SHARED_AUDIO_INTERFACE audioIface; A04_SHARED_USB_INTERFACE usbIface;
    A04_SHARED_WIFI_INTERFACE wifiIface;
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
#if A04_SHARED_FIRST_PLAYBACK
        c->audio.preflight=ParentFirstPreflight;c->audio.verify_release=ParentPinnedRelease;
        c->classd.verify_start=ParentFirstCodecReady;c->classd.verify_quiescent=ParentPinnedRelease;
        c->classd.verify_unmute=ParentFirstUnmuteReady;
#endif
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
#if A04_USB_GPIO22_SOURCE_EXPERIMENT
        c->usb.probe_source=ParentUsbProbeSource;c->usb.otg_acquire=ParentUsbSourceAcquire;c->usb.otg_release=ParentUsbSourceRelease;
#endif
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
#if A04_SHARED_GPU_PLATFORM
    status=ParentGpuRegister(dev);if(!NT_SUCCESS(status))return status;
#endif
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
    thermalIface.VerifyHeadphone=ParentThermalVerifyHeadphone;
    thermalIface.VerifyOwnership=ParentThermalVerifyOwnership;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&thermalIface,&A04SharedThermalInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
#endif
#if A04_SHARED_CAPTURE
    {A04_CAPTURE_INTERFACE capture;RtlZeroMemory(&capture,sizeof(capture));
     ParentCaptureInitialize(ParentContext(dev));
     capture.Header.Size=sizeof(capture);capture.Header.Version=A04_CAPTURE_INTERFACE_VERSION;
     capture.Header.Context=dev;capture.Header.InterfaceReference=ParentInterfaceReference;
     capture.Header.InterfaceDereference=ParentInterfaceDereference;
     capture.AcquireInput=ParentCaptureAcquire;capture.PrepareMaskedIrq=ParentAudioPrepareIrq;
     capture.ReleaseUnexposed=ParentCaptureRelease;capture.QueryLatency=ParentCaptureLatency;
     WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&capture,&A04CaptureInterfaceGuid,NULL);
     status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;}
#endif
#if A04_SHARED_THERMAL_NATIVE_EXPERIMENT
    RtlZeroMemory(&nativeIface,sizeof(nativeIface));
    nativeIface.Header.Size=sizeof(nativeIface);nativeIface.Header.Version=MT8167_THERMAL_PARENT_PIO_VERSION;
    nativeIface.Header.Context=dev;nativeIface.Header.InterfaceReference=ParentInterfaceReference;nativeIface.Header.InterfaceDereference=ParentInterfaceDereference;
    nativeIface.ParentBuild=MT8167_THERMAL_PARENT_PIO_BUILD;
    nativeIface.Measure=ParentThermalPioMeasure;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&nativeIface,&Mt8167ThermalParentPioGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
#endif

    RtlZeroMemory(&audioIface,sizeof(audioIface));
    audioIface.Header.Size=sizeof(audioIface);audioIface.Header.Version=A04_SHARED_AUDIO_INTERFACE_VERSION;
    audioIface.Header.Context=dev;audioIface.Header.InterfaceReference=ParentInterfaceReference;
    audioIface.Header.InterfaceDereference=ParentInterfaceDereference;
    #if A04_SHARED_VOICE_PCM_DIAGNOSTIC
    audioIface.VerifyDmaDrained=ParentVerifyDmaDrained;audioIface.VerifyLivePcm=ParentVerifyLivePcm;audioIface.StartVoice=ParentStartVoice;audioIface.StopVoice=ParentStopVoice;
#endif
    #if A04_SHARED_DIGITAL_PLAYBACK
    audioIface.PreparePlayback=ParentPreparePlayback;
#if A04_SHARED_FIRST_PLAYBACK
    audioIface.ReadDiagnostic=ParentReadAudioDiagnostic;
#endif
#endif
    audioIface.BindAfe=ParentBindAfe;audioIface.Mute=ParentAudioMute;audioIface.UnmuteMin=ParentAudioUnmute;audioIface.AcquireMuted=ParentAudioAcquire;audioIface.ReleaseQuiescent=ParentAudioRelease;audioIface.PrepareMaskedIrq=ParentAudioPrepareIrq;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&audioIface,&A04SharedAudioInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&usbIface,sizeof(usbIface));usbIface.Header.Size=sizeof(usbIface);usbIface.Header.Version=A04_SHARED_USB_INTERFACE_VERSION;
    usbIface.Header.Context=dev;usbIface.Header.InterfaceReference=ParentInterfaceReference;usbIface.Header.InterfaceDereference=ParentInterfaceDereference;
    usbIface.Acquire=ParentUsbAcquire;usbIface.SetVbus=ParentUsbVbus;usbIface.Health=ParentUsbHealth;usbIface.Release=ParentUsbRelease;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&usbIface,&A04SharedUsbInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&wifiIface,sizeof(wifiIface));
    wifiIface.Header.Size=sizeof(wifiIface);wifiIface.Header.Version=A04_SHARED_WIFI_INTERFACE_VERSION;
    wifiIface.Header.Context=dev;wifiIface.Header.InterfaceReference=ParentInterfaceReference;
    wifiIface.Header.InterfaceDereference=ParentInterfaceDereference;
    wifiIface.Acquire=ParentWifiAcquire;wifiIface.ReleaseQuiescent=ParentWifiRelease;
    wifiIface.Live=ParentWifiLive;wifiIface.UpdateShared=ParentWifiUpdate;wifiIface.WriteShared=ParentWifiWrite;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&wifiIface,&A04SharedWifiInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
#endif
#if A04_SHARED_SD_POWER
    if(!NT_SUCCESS(status))return status;
    status=ParentSdPublish(dev);if(!NT_SUCCESS(status))return status;
#endif
#if defined(A04_SHARED_CHILD_PROBE) && A04_SHARED_CHILD_PROBE
    if(!NT_SUCCESS(status))return status;
#if !A04_SHARED_USB_ONLY
#if MT6392_RTC_CALENDAR
    RtlZeroMemory(&rtcIface,sizeof(rtcIface));
    rtcIface.Header.Size=sizeof(rtcIface);rtcIface.Header.Version=MT6392_RTC_INTERFACE_VERSION;
    rtcIface.Header.Context=dev;rtcIface.Header.InterfaceReference=ParentInterfaceReference;
    rtcIface.Header.InterfaceDereference=ParentInterfaceDereference;
    rtcIface.ReadCalendar=Mt6392ReadCalendar;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&rtcIface,&Mt6392RtcInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"MT6392\\RTC",L"0",0xfffffffdu);
    if(!NT_SUCCESS(status))return status;
#endif
    status=ParentAddProbeChild(dev,L"MT6392\\Buttons",L"0",0x10002000u);
    if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"MT8167\\BU21025",L"0",0x11009000u);
    if(!NT_SUCCESS(status))return status;
#if A04_SHARED_THERMAL_DIAGNOSTIC
    status=ParentAddProbeChild(dev,L"MT8167\\Thermal",L"0",0xfffffffeu);
    if(!NT_SUCCESS(status))return status;
#endif
#endif
    status=ParentAddProbeChild(dev,L"MT8167\\USB20",L"0",0x11100000u);
    if(!NT_SUCCESS(status))return status;
#if !A04_SHARED_USB_ONLY
    /* The audio sentinel remains unavailable unless the diagnostic flag is set. */
    status=ParentAddProbeChild(dev,L"MT8167\\AFE",L"0",0xffffffffu);
    if(!NT_SUCCESS(status))return status;
    /* The host MMIO range is a separate child resource. The optional MSDC2 interrupt descriptor still needs platform polarity and
     * translation verification; it does not describe the separate EINT5. */
    status=ParentAddProbeChild(dev,L"A04BR3\\MT6630SDIO",L"0",0x11170000u);
#endif
#endif
#if A04_SHARED_ACCDET_READ
    if(!NT_SUCCESS(status))return status;
    status=ParentAccdetPublish(dev);
#endif
    return status;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,ParentAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}

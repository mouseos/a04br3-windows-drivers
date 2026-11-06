/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Optional WDF SDIO diagnostic. Hardware path is compiled and provisioned
 * only by explicit flags and verified board configuration; default is off.
 */
#include <ntddk.h>
#include <wdf.h>
#include "../../bus/mt8167-shared/wifi_interface.h"
#ifndef A04_WIFI_WDF_LEASE_DIAGNOSTIC
#define A04_WIFI_WDF_LEASE_DIAGNOSTIC 0
#endif
#ifndef A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
#define A04_WIFI_WDF_HARDWARE_DIAGNOSTIC 0
#endif
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC && !A04_WIFI_WDF_LEASE_DIAGNOSTIC
#error Hardware diagnostic requires the parent interface
#endif
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
#if !defined(A04_WIFI_SESSION_DIAGNOSTIC) || !A04_WIFI_SESSION_DIAGNOSTIC || !defined(A04_WIFI_IRQ_DIAGNOSTIC) || !A04_WIFI_IRQ_DIAGNOSTIC
#error Hardware diagnostic requires session and IRQ support
#endif
#include "windows_session_irq.h"
#include "windows_irq_firmware.h"
#ifndef A04_WIFI_WDF_CONFIG_HEADER
#define A04_WIFI_WDF_CONFIG_HEADER "wdf_hardware_config.h"
#endif
#include A04_WIFI_WDF_CONFIG_HEADER
#endif
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
typedef struct _WIFI_CONTEXT {
 A04_SHARED_WIFI_INTERFACE parent;
 int referenced;
 /* A provider contract violation or failed release must not be mistaken for
  * a clean stop. Keep the parent interface reference in that case. It pins
  * the object only, not PnP resources, and cannot make an unsafe lease safe. */
 int uncertain;
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 CM_PARTIAL_RESOURCE_DESCRIPTOR raw_mem, translated_mem, raw_irq, translated_irq;
 int resources_valid, removed;
 KEVENT serial;
 struct a04_windows_session session;
 struct a04_windows_session_irq session_irq;
 struct a04_windows_session_platform platform;
 struct a04_irq_work irq_work;
 struct a04_irq_firmware firmware_irq;
#endif
} WIFI_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WIFI_CONTEXT,WifiContext)
#endif
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
/* PASSIVE_LEVEL PnP and worker serialization; worker never waits on stop. */
static void wifi_lock(WIFI_CONTEXT *c)
{ (void)KeWaitForSingleObject(&c->serial,Executive,KernelMode,FALSE,NULL); }
static void wifi_unlock(void *p)
{ WIFI_CONTEXT *c=p; KeSetEvent(&c->serial,IO_NO_INCREMENT,FALSE); }
static int wifi_try_lock(void *p)
{ WIFI_CONTEXT *c=p; LARGE_INTEGER zero; zero.QuadPart=0;
 return KeWaitForSingleObject(&c->serial,Executive,KernelMode,FALSE,&zero)==STATUS_SUCCESS?0:-1; }
static int wifi_no_other_io(void *p) { UNREFERENCED_PARAMETER(p); return 0; }
/* No verified combo power-off/firmware stop provider is available yet. */
static int wifi_combo_not_quiescent(void *p) { UNREFERENCED_PARAMETER(p); return -1; }
static int wifi_firmware_poll(void *p)
{
 struct a04_irq_firmware *f=p;int result=a04_irq_firmware_poll(f);
 if(!result)DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,
  "A04WIFI: one-shot WLAN interrupt status 0x%08lx\n",(unsigned long)f->observed);
 return result;
}
static int wifi_resources(WIFI_CONTEXT *c,WDFCMRESLIST raw,WDFCMRESLIST trans)
{
 ULONG i,n; int mem=0,irq=0;
 if(!raw||!trans||(n=WdfCmResourceListGetCount(raw))!=WdfCmResourceListGetCount(trans))return -1;
 for(i=0;i<n;i++) {
  PCM_PARTIAL_RESOURCE_DESCRIPTOR r=WdfCmResourceListGetDescriptor(raw,i);
  PCM_PARTIAL_RESOURCE_DESCRIPTOR t=WdfCmResourceListGetDescriptor(trans,i);
  if(!r||!t||r->Type!=t->Type)return -1;
  if(r->Type==CmResourceTypeMemory) {
   if(mem++||r->u.Memory.Start.QuadPart!=0x11170000 || r->u.Memory.Length!=0x1000 || t->u.Memory.Length!=0x1000 ||
      r->ShareDisposition!=CmResourceShareDeviceExclusive || t->ShareDisposition!=CmResourceShareDeviceExclusive)return -1;
   c->raw_mem=*r;c->translated_mem=*t;
  } else if(r->Type==CmResourceTypeInterrupt) {
   if(irq++||r->u.Interrupt.Vector!=141 || !t->u.Interrupt.Affinity ||
      r->ShareDisposition!=CmResourceShareDeviceExclusive || t->ShareDisposition!=CmResourceShareDeviceExclusive ||
      (r->Flags&CM_RESOURCE_INTERRUPT_LATCHED)||(t->Flags&CM_RESOURCE_INTERRUPT_LATCHED))return -1;
   c->raw_irq=*r;c->translated_irq=*t;
  } else return -1;
 }
 return mem==1&&irq==1?0:-1;
}
/* No device registers on surprise removal. Always disconnect and drain even
 * after failed startup/stop, before the WDF context can be destroyed. Failure
 * retains the parent reference and lease; it does not assert PnP resource safety. */
static void wifi_disconnect(WIFI_CONTEXT *c)
{
 if(c->session_irq.irq.interrupt && a04_windows_irq_remove(&c->session_irq.irq))c->uncertain=1;
 if(c->irq_work.work && a04_irq_work_drain(&c->irq_work))c->uncertain=1;
}
static int wifi_stop(WIFI_CONTEXT *c)
{
 int result=0;
 if(c->session.state==A04_SESSION_READY || c->session.state==A04_SESSION_QUARANTINED) {
  if(c->removed || a04_windows_session_stop(&c->session))result=-1;
 }
 if(c->session.state==A04_SESSION_STARTING)result=-1;
 wifi_disconnect(c);
 if(result || c->session.state==A04_SESSION_QUARANTINED || c->uncertain) {
  c->uncertain=1;return -1;
 }
 return 0;
}
static int wifi_config_valid(void)
{
 unsigned i,nonzero=0;
 for(i=0;i<sizeof(a04_wifi_wdf_wmt_sha256);i++)nonzero|=a04_wifi_wdf_wmt_sha256[i];
 return A04_WIFI_WDF_CONFIG_VERIFIED && nonzero && a04_wifi_wdf_ocr && !(a04_wifi_wdf_ocr&~0x00ff8000u) &&
  a04_wifi_wdf_options.coex_present<=1 && a04_wifi_wdf_options.antenna_mode<=255 &&
  a04_wifi_wdf_options.co_clock<=1 && a04_wifi_wdf_options.merge_interface<=1 &&
  (a04_wifi_wdf_options.fm_strap==1 || a04_wifi_wdf_options.fm_strap==2) && a04_wifi_wdf_options.function1<=1;
}
#endif
EVT_WDF_DRIVER_DEVICE_ADD WifiAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE WifiPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE WifiRelease;
EVT_WDF_DEVICE_D0_ENTRY WifiD0Entry;
EVT_WDF_DEVICE_D0_EXIT WifiD0Exit;
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
EVT_WDF_DEVICE_SURPRISE_REMOVAL WifiSurpriseRemoval;
#endif
NTSTATUS WifiPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 WIFI_CONTEXT *rc=WifiContext(dev);
 if(wifi_resources(rc,raw,translated))return STATUS_DEVICE_CONFIGURATION_ERROR;
 rc->resources_valid=1;rc->removed=0;
#else
 UNREFERENCED_PARAMETER(raw);UNREFERENCED_PARAMETER(translated);
#endif
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);NTSTATUS status;
 if(c->referenced||c->uncertain)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&c->parent,sizeof(c->parent));
 status=WdfFdoQueryForInterface(dev,&A04SharedWifiInterfaceGuid,(PINTERFACE)&c->parent,
  sizeof(c->parent),A04_SHARED_WIFI_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(status))return status;
 if(!c->parent.Header.InterfaceDereference)return STATUS_DEVICE_CONFIGURATION_ERROR;
 c->referenced=1;
 if(c->parent.Header.Size<sizeof(c->parent)||c->parent.Header.Version!=A04_SHARED_WIFI_INTERFACE_VERSION||
    !c->parent.Acquire||!c->parent.ReleaseQuiescent||!c->parent.Live||!c->parent.UpdateShared||!c->parent.WriteShared)
  return STATUS_DEVICE_CONFIGURATION_ERROR;
 return STATUS_SUCCESS;
#else
 UNREFERENCED_PARAMETER(dev);return STATUS_DEVICE_NOT_READY;
#endif
}
NTSTATUS WifiD0Entry(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
 UNREFERENCED_PARAMETER(previous);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);NTSTATUS status;
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 wifi_lock(c);
 if(!c->referenced||c->uncertain||c->removed||!c->resources_valid||!wifi_config_valid()||
    c->irq_work.work||c->session_irq.irq.interrupt||
    (c->session.state!=A04_SESSION_OFF && c->session.state!=A04_SESSION_STOPPED && c->session.state!=A04_SESSION_REJECTED)) {
  wifi_unlock(c);return STATUS_DEVICE_NOT_READY;
 }
 RtlZeroMemory(&c->session,sizeof(c->session));RtlZeroMemory(&c->session_irq,sizeof(c->session_irq));
 RtlZeroMemory(&c->irq_work,sizeof(c->irq_work));RtlZeroMemory(&c->firmware_irq,sizeof(c->firmware_irq));
 c->firmware_irq.session=&c->session;c->firmware_irq.context=c;
 c->firmware_irq.try_lock=wifi_try_lock;c->firmware_irq.unlock=wifi_unlock;
 status=a04_irq_work_init(&c->irq_work,WdfDeviceWdmGetDeviceObject(dev),wifi_firmware_poll,&c->firmware_irq);
 if(!NT_SUCCESS(status)){wifi_unlock(c);return status;}
 c->session_irq.raw=&c->raw_irq;c->session_irq.translated=&c->translated_irq;
 c->session_irq.work=&c->irq_work;c->session_irq.context=c;c->session_irq.drain_io=wifi_no_other_io;
 c->session_irq.quiesce_combo=wifi_combo_not_quiescent;
 if(a04_windows_session_irq_bind(&c->session_irq,&c->platform) ||
    a04_windows_session_start(&c->session,&c->parent,&c->platform,&c->raw_mem,&c->translated_mem,
      a04_wifi_wdf_ocr,a04_wifi_wdf_wmt_sha256,&a04_wifi_wdf_options)) {
  (void)wifi_stop(c);wifi_unlock(c);return STATUS_DEVICE_NOT_READY;
 }
 if(a04_windows_irq_arm(&c->session_irq.irq)) {
  (void)wifi_stop(c);wifi_unlock(c);return STATUS_DEVICE_NOT_READY;
 }
 wifi_unlock(c);return STATUS_SUCCESS;
#else
 ULONG generation=0;
 if(!c->referenced||c->uncertain)return STATUS_DEVICE_NOT_READY;
 status=c->parent.Acquire(c->parent.Header.Context,&generation);
 if(!NT_SUCCESS(status)){
  DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,"A04WIFI: parent rejected lease: 0x%08lx\n",(unsigned long)status);
  return status; /* contract: failure has no effects */
 }
 /* Nothing has touched hardware. Do not enable the data path while forced
  * removal and lease/MMIO/IRQ rundown are unproven. */
 if(!generation || !NT_SUCCESS(c->parent.ReleaseQuiescent(c->parent.Header.Context,generation)))
  c->uncertain=1;
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,"A04WIFI: lease granted; startup refused; release %s\n",c->uncertain?"uncertain":"completed");
 return STATUS_DEVICE_NOT_READY;
#endif /* hardware diagnostic */
#else
 UNREFERENCED_PARAMETER(dev);return STATUS_DEVICE_NOT_READY;
#endif
}
NTSTATUS WifiD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{
 UNREFERENCED_PARAMETER(target);
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);int result;wifi_lock(c);result=wifi_stop(c);wifi_unlock(c);
 return result?STATUS_DEVICE_NOT_READY:STATUS_SUCCESS;
#else
 UNREFERENCED_PARAMETER(dev);return STATUS_SUCCESS;
#endif
}
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
VOID WifiSurpriseRemoval(WDFDEVICE dev)
{
 WIFI_CONTEXT *c=WifiContext(dev);wifi_lock(c);c->removed=1;
 if(c->session.state!=A04_SESSION_OFF && c->session.state!=A04_SESSION_REJECTED && c->session.state!=A04_SESSION_STOPPED)c->uncertain=1;
 wifi_disconnect(c);wifi_unlock(c);
}
#endif
NTSTATUS WifiRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
 UNREFERENCED_PARAMETER(translated);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WIFI_CONTEXT *c=WifiContext(dev);
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 wifi_lock(c);(void)wifi_stop(c);c->resources_valid=0;wifi_unlock(c);
#endif
 if(c->referenced&&!c->uncertain){c->parent.Header.InterfaceDereference(c->parent.Header.Context);c->referenced=0;}
#else
 UNREFERENCED_PARAMETER(dev);
#endif
 return STATUS_SUCCESS;
}
NTSTATUS WifiAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDFDEVICE dev;WDF_OBJECT_ATTRIBUTES attr;
 UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);
 callbacks.EvtDevicePrepareHardware=WifiPrepare;callbacks.EvtDeviceReleaseHardware=WifiRelease;
 callbacks.EvtDeviceD0Entry=WifiD0Entry;callbacks.EvtDeviceD0Exit=WifiD0Exit;
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 callbacks.EvtDeviceSurpriseRemoval=WifiSurpriseRemoval;
#endif
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
#if A04_WIFI_WDF_LEASE_DIAGNOSTIC
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,WIFI_CONTEXT);
#else
 WDF_OBJECT_ATTRIBUTES_INIT(&attr);
#endif
 attr.ExecutionLevel=WdfExecutionLevelPassive;
 {
 NTSTATUS status=WdfDeviceCreate(&init,&attr,&dev);
#if A04_WIFI_WDF_HARDWARE_DIAGNOSTIC
 if(NT_SUCCESS(status))KeInitializeEvent(&WifiContext(dev)->serial,SynchronizationEvent,TRUE);
#endif
 return status;
 }
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,WifiAdd);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);}

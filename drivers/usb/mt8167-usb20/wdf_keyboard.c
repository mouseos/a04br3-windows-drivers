/* Diagnostic HID transport for one MT8167 USB boot keyboard. This is not a
 * general USB host driver. It is not packaged until a board owner connects
 * KbHidReport/KbHidDisconnected to a qualified MUSB controller lifetime. */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "keyboard.h"
#include "keyboard_session.h"
#include "wdf_controller.h"
#include "wdf_keyboard.h"
#include "../../bus/mt8167-shared/usb_interface.h"
#define REPORT_SLOTS 32u
typedef struct {
 WDFWAITLOCK lock; WDFQUEUE reads; UCHAR reports[REPORT_SLOTS][8];
 unsigned head,count; BOOLEAN connected,overflow,draining; KB_SESSION session; A04_USB_WDF *usb; WDFWAITLOCK controller_lock; WDFTIMER poll; BOOLEAN rx_pending, board_owned; A04_USB_BOARD board; A04_SHARED_USB_INTERFACE shared; BOOLEAN shared_valid; ULONGLONG next_poll;
} KB_HID_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(KB_HID_CONTEXT,KbHidContext)
EVT_WDF_DRIVER_DEVICE_ADD KbHidAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE KbPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE KbRelease;
EVT_WDF_DEVICE_D0_EXIT KbD0Exit;
EVT_WDF_DEVICE_D0_ENTRY KbD0;
EVT_WDF_TIMER KbPoll;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL KbHidIoctl;
static NTSTATUS CopyOutput(WDFREQUEST request,const void *p,size_t n)
{
 PIRP irp=WdfRequestWdmGetIrp(request);
 PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
 if(!irp->UserBuffer || stack->Parameters.DeviceIoControl.OutputBufferLength<n)
  return STATUS_BUFFER_TOO_SMALL;
 /* HIDClass supplies a system-resident buffer for these internal IOCTLs. */
 RtlCopyMemory(irp->UserBuffer,p,n); WdfRequestSetInformation(request,n);
 return STATUS_SUCCESS;
}
static void Drain(WDFDEVICE dev)
{
 KB_HID_CONTEXT *c=KbHidContext(dev); WDFREQUEST r; UCHAR report[8]; NTSTATUS status;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->draining) { WdfWaitLockRelease(c->lock); return; }
 c->draining=TRUE; WdfWaitLockRelease(c->lock);
 for(;;) {
  WdfWaitLockAcquire(c->lock,NULL);
  if(!c->count || !NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads,&r))) {
   c->draining=FALSE; WdfWaitLockRelease(c->lock); return;
  }
  RtlCopyMemory(report,c->reports[c->head],8); c->head=(c->head+1u)%REPORT_SLOTS; --c->count;
  WdfWaitLockRelease(c->lock);
  status=CopyOutput(r,report,8); WdfRequestComplete(r,status);
 }
}
/* Called at PASSIVE_LEVEL after a complete eight-byte boot report. */
NTSTATUS KbHidReport(WDFDEVICE dev,const UCHAR report[8])
{
 KB_HID_CONTEXT *c=KbHidContext(dev); unsigned slot;
 if(!report)return STATUS_INVALID_PARAMETER;
 WdfWaitLockAcquire(c->lock,NULL);
 if(c->overflow) { WdfWaitLockRelease(c->lock); return STATUS_DEVICE_DATA_ERROR; }
 if(c->count==REPORT_SLOTS) { c->overflow=TRUE; WdfWaitLockRelease(c->lock); WdfIoQueuePurge(c->reads,WDF_NO_EVENT_CALLBACK,WDF_NO_CONTEXT); return STATUS_BUFFER_OVERFLOW; }
 slot=(c->head+c->count)%REPORT_SLOTS; RtlCopyMemory(c->reports[slot],report,8); ++c->count; c->connected=TRUE;
 WdfWaitLockRelease(c->lock); Drain(dev); return STATUS_SUCCESS;
}
/* On unplug queue a release report so a key cannot remain held. */
void KbHidDisconnected(WDFDEVICE dev)
{
 KB_HID_CONTEXT *c=KbHidContext(dev); static const UCHAR release[8]={0};
 WdfWaitLockAcquire(c->lock,NULL); c->head=0; c->count=0; c->overflow=FALSE; c->connected=FALSE;
 RtlCopyMemory(c->reports[0],release,8); c->count=1;
 WdfWaitLockRelease(c->lock); WdfIoQueueStart(c->reads); Drain(dev);
}
/* Controller callbacks run at PASSIVE_LEVEL with external serialization.
 * They are never started by this HID filter without an owning board driver. */
static void SessionDelay(void *p,unsigned ms)
{ LARGE_INTEGER interval; UNREFERENCED_PARAMETER(p); interval.QuadPart=-10000LL*ms; (void)KeDelayExecutionThread(KernelMode,FALSE,&interval); }
static int SessionReport(void *p,const unsigned char *report)
{ return NT_SUCCESS(KbHidReport((WDFDEVICE)p,report)) ? 0 : -1; }
int KbHidControllerStart(WDFDEVICE dev,const MT8167_USB20_IO *io,unsigned speed)
{ return KbSessionStart(&KbHidContext(dev)->session,io,speed,dev,SessionDelay,SessionReport); }
int KbHidControllerEp0Interrupt(WDFDEVICE dev)
{ return KbSessionEp0Interrupt(&KbHidContext(dev)->session); }
int KbHidControllerRequest(WDFDEVICE dev)
{ return KbSessionRequestReport(&KbHidContext(dev)->session); }
int KbHidControllerRxInterrupt(WDFDEVICE dev)
{ return KbSessionRxInterrupt(&KbHidContext(dev)->session); }
void KbHidControllerDisconnect(WDFDEVICE dev)
{ KbHidDisconnected(dev); }
/* IRQ work is PASSIVE_LEVEL. Disconnect wins over stale endpoint events.
 * The platform owner must arm after acquiring the shared power lease. */
static unsigned PollMs(const KB_SESSION *s)
{
 unsigned n=s->enumeration.interval;
 if(s->speed!=0x40)return n;
 if(n>16)n=16;
 return ((1u<<(n-1))+7u)/8u;
}
VOID KbPoll(WDFTIMER timer)
{
 WDFDEVICE dev=(WDFDEVICE)WdfTimerGetParentObject(timer); KB_HID_CONTEXT*c=KbHidContext(dev); ULONGLONG now;
 WdfWaitLockAcquire(c->controller_lock,NULL);
 if(c->board_owned && c->shared_valid) {
  if(c->shared.Health(c->shared.Header.Context)) {
   c->session.enumeration.stage=KB_ERROR; c->rx_pending=FALSE; KbHidDisconnected(dev);
   WdfWaitLockRelease(c->controller_lock); return;
  }
  now=KeQueryInterruptTime();
  if(c->session.enumeration.stage==KB_READY && !c->rx_pending && now>=c->next_poll) {
   if(KbHidControllerRequest(dev)) { c->session.enumeration.stage=KB_ERROR; KbHidDisconnected(dev); }
   else { c->rx_pending=TRUE; c->next_poll=now+((ULONGLONG)PollMs(&c->session)*10000u); }
  }
  WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(10));
 }
 WdfWaitLockRelease(c->controller_lock);
}
static void KbControllerEvents(void *context,MT8167_USB20_EVENTS events)
{
 WDFDEVICE dev=(WDFDEVICE)context; KB_HID_CONTEXT*c=KbHidContext(dev); int result;
 const MT8167_USB20_CONTROLLER *controller; unsigned char power,devctl; unsigned speed;
 WdfWaitLockAcquire(c->controller_lock,NULL);
 if(events.usb & (0x20u|0x80u)) {
  c->rx_pending=FALSE; KbHidDisconnected(dev); c->session.enumeration.stage=KB_ERROR;
 } else {
  if(events.usb & 0x10u) {
   controller=A04UsbWdfController(c->usb);
   if(controller) {
    /* Assert USB bus reset for 50 ms; avoid touching unrelated POWER bits. */
    power=controller->io.read8(controller->io.context,1);
    controller->io.write8(controller->io.context,1,(unsigned char)(power|0x08));
    SessionDelay(dev,50);
    power=controller->io.read8(controller->io.context,1);
    controller->io.write8(controller->io.context,1,(unsigned char)(power&~0x08));
    SessionDelay(dev,10);
    power=controller->io.read8(controller->io.context,1);
    devctl=controller->io.read8(controller->io.context,0x60);
    speed=(devctl&0x20)?0xc0:((power&0x10)?0x40:0x80);
    if((devctl&0x80) || KbHidControllerStart(dev,&controller->io,speed))
      c->session.enumeration.stage=KB_ERROR;
   }
   /* EP0 status from a previous connection must never reach the new session. */
   events.tx=events.rx=0; c->rx_pending=FALSE;
  }
  if((events.tx&1u) && c->session.enumeration.stage!=KB_ERROR) {
   result=KbHidControllerEp0Interrupt(dev);
   if(result<0) { c->session.enumeration.stage=KB_ERROR; KbHidDisconnected(dev); }
   else if(result==1)c->next_poll=0;
  }
  if((events.rx&2u) && c->session.enumeration.stage==KB_READY && c->rx_pending) {
   result=KbHidControllerRxInterrupt(dev);
   if(result==1 || result==2)c->rx_pending=FALSE;
   if(result<0) { c->session.enumeration.stage=KB_ERROR; KbHidDisconnected(dev); }
  }
 }
 WdfWaitLockRelease(c->controller_lock);
}
static int BoardPower(void *p,int on)
{ KB_HID_CONTEXT*c=KbHidContext((WDFDEVICE)p); return on?c->shared.Acquire(c->shared.Header.Context):c->shared.Release(c->shared.Header.Context); }
static int BoardVbus(void *p,int on)
{ KB_HID_CONTEXT*c=KbHidContext((WDFDEVICE)p); return c->shared.SetVbus(c->shared.Header.Context,on); }
static unsigned char BoardPhyRead(void *p,unsigned off)
{ return A04UsbWdfPhyRead(KbHidContext((WDFDEVICE)p)->usb,off); }
static void BoardPhyWrite(void *p,unsigned off,unsigned char val)
{ A04UsbWdfPhyWrite(KbHidContext((WDFDEVICE)p)->usb,off,val); }
static unsigned char BoardMacRead(void *p,unsigned off)
{ const MT8167_USB20_CONTROLLER*c=A04UsbWdfController(KbHidContext((WDFDEVICE)p)->usb);return c->io.read8(c->io.context,off); }
static void BoardMacWrite(void *p,unsigned off,unsigned char val)
{ const MT8167_USB20_CONTROLLER*c=A04UsbWdfController(KbHidContext((WDFDEVICE)p)->usb);c->io.write8(c->io.context,off,val); }
NTSTATUS KbHidActivate(WDFDEVICE dev,const A04_USB_BOARD *board)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); NTSTATUS status;
 if(!board || !A04UsbWdfController(c->usb) || c->board_owned)return STATUS_INVALID_DEVICE_STATE;
 c->board=*board;
 if(A04UsbHostStart(&c->board))return STATUS_DEVICE_POWER_FAILURE;
 c->board_owned=TRUE;
 status=A04UsbWdfArm(c->usb);
 if(!NT_SUCCESS(status)) { A04UsbHostStop(&c->board); c->board_owned=FALSE; }
 return status;
}
void KbHidDeactivate(WDFDEVICE dev)
{
 KB_HID_CONTEXT*c=KbHidContext(dev);
 WdfTimerStop(c->poll,TRUE); A04UsbWdfDisarm(c->usb);
 WdfWaitLockAcquire(c->controller_lock,NULL);
 c->session.enumeration.stage=KB_ERROR; c->rx_pending=FALSE;
 KbHidDisconnected(dev);
 if(c->board_owned) { A04UsbHostStop(&c->board); c->board_owned=FALSE; }
 WdfWaitLockRelease(c->controller_lock);
}
NTSTATUS KbPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); NTSTATUS status;
 UNREFERENCED_PARAMETER(raw);
 status=A04UsbWdfPrepare(c->usb,translated); if(!NT_SUCCESS(status))return status;
 RtlZeroMemory(&c->shared,sizeof(c->shared));
 status=WdfFdoQueryForInterface(dev,&A04SharedUsbInterfaceGuid,(PINTERFACE)&c->shared,sizeof(c->shared),A04_SHARED_USB_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(status)){A04UsbWdfRelease(c->usb);return status;}
 if(c->shared.Header.Size<sizeof(c->shared) || c->shared.Header.Version!=A04_SHARED_USB_INTERFACE_VERSION || !c->shared.Header.InterfaceDereference || !c->shared.Acquire || !c->shared.SetVbus || !c->shared.Health || !c->shared.Release) {
  if(c->shared.Header.InterfaceDereference)c->shared.Header.InterfaceDereference(c->shared.Header.Context);
  A04UsbWdfRelease(c->usb);return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 c->shared_valid=TRUE; return STATUS_SUCCESS;
}
NTSTATUS KbD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); A04_USB_BOARD b; UNREFERENCED_PARAMETER(previous);
 if(!c->shared_valid)return STATUS_DEVICE_NOT_READY;
 RtlZeroMemory(&b,sizeof(b)); b.context=dev; b.power=BoardPower; b.vbus=BoardVbus;
 b.phy_read=BoardPhyRead; b.phy_write=BoardPhyWrite; b.mac_read=BoardMacRead; b.mac_write=BoardMacWrite; b.delay_ms=SessionDelay;
 { NTSTATUS status=KbHidActivate(dev,&b);
   if(NT_SUCCESS(status))WdfTimerStart(c->poll,WDF_REL_TIMEOUT_IN_MS(10));
   return status;
 }
}
NTSTATUS KbD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{ UNREFERENCED_PARAMETER(target); KbHidDeactivate(dev); return STATUS_SUCCESS; }
NTSTATUS KbRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); UNREFERENCED_PARAMETER(translated); KbHidDeactivate(dev);
 if(c->shared_valid) { c->shared.Header.InterfaceDereference(c->shared.Header.Context); c->shared_valid=FALSE; }
 A04UsbWdfRelease(c->usb); return STATUS_SUCCESS;
}
VOID KbHidIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 WDFDEVICE dev=WdfIoQueueGetDevice(queue); KB_HID_CONTEXT *c=KbHidContext(dev);
 HID_DESCRIPTOR h={0}; HID_DEVICE_ATTRIBUTES attr={0}; const unsigned char *desc; unsigned len; NTSTATUS status=STATUS_NOT_SUPPORTED;
 UNREFERENCED_PARAMETER(outlen); UNREFERENCED_PARAMETER(inlen);
 switch(code) {
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
  (void)KbHidDescriptor(&len); h.bLength=sizeof(h); h.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;
  h.bcdHID=0x0111; h.bNumDescriptors=1; h.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;
  h.DescriptorList[0].wReportLength=(USHORT)len; status=CopyOutput(request,&h,sizeof(h)); break;
 case IOCTL_HID_GET_REPORT_DESCRIPTOR: desc=KbHidDescriptor(&len); status=CopyOutput(request,desc,len); break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES: attr.Size=sizeof(attr); attr.VersionNumber=1; status=CopyOutput(request,&attr,sizeof(attr)); break;
 case IOCTL_HID_READ_REPORT:
  status=WdfRequestForwardToIoQueue(request,c->reads); if(NT_SUCCESS(status)) { Drain(dev); return; } break;
 }
 WdfRequestComplete(request,status);
}
NTSTATUS KbHidAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDF_OBJECT_ATTRIBUTES attr,child; WDF_IO_QUEUE_CONFIG q; WDF_TIMER_CONFIG timer; WDFDEVICE dev; KB_HID_CONTEXT *c; NTSTATUS s; WDF_PNPPOWER_EVENT_CALLBACKS pnp;
 UNREFERENCED_PARAMETER(driver); WdfFdoInitSetFilter(init);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp); pnp.EvtDevicePrepareHardware=KbPrepare; pnp.EvtDeviceReleaseHardware=KbRelease; pnp.EvtDeviceD0Entry=KbD0; pnp.EvtDeviceD0Exit=KbD0Exit;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,KB_HID_CONTEXT); attr.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfDeviceCreate(&init,&attr,&dev); if(!NT_SUCCESS(s))return s; c=KbHidContext(dev);
 WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject=dev; s=WdfWaitLockCreate(&child,&c->lock); if(!NT_SUCCESS(s))return s;
 s=WdfWaitLockCreate(&child,&c->controller_lock); if(!NT_SUCCESS(s))return s;
 s=A04UsbWdfCreate(dev,KbControllerEvents,dev,&c->usb); if(!NT_SUCCESS(s))return s;
 c->session.enumeration.stage=KB_ERROR;
 WDF_TIMER_CONFIG_INIT(&timer,KbPoll); timer.AutomaticSerialization=FALSE;
 WDF_OBJECT_ATTRIBUTES_INIT(&child); child.ParentObject=dev; child.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfTimerCreate(&timer,&child,&c->poll); if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel); q.EvtIoInternalDeviceControl=KbHidIoctl;
 s=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE); if(!NT_SUCCESS(s))return s;
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual); return WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{ WDF_DRIVER_CONFIG c; WDF_DRIVER_CONFIG_INIT(&c,KbHidAdd); return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }

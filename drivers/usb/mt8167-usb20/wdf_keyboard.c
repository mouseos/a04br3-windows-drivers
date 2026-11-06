/* Diagnostic HID transport for one MT8167 USB boot keyboard. This is not a
 * general USB host driver. It is not packaged until a board owner connects
 * KbHidReport/KbHidDisconnected to a qualified MUSB controller lifetime. */
#include <ntddk.h>
#include <wdf.h>
#include <hidport.h>
#include "keyboard.h"
#include "keyboard_session.h"
#include "keyboard_port.h"
#include "wdf_controller.h"
#include "wdf_keyboard.h"
#include "../../bus/mt8167-shared/usb_interface.h"
#ifndef A04_MUSB_HID_CONTROL_EXPERIMENT
#define A04_MUSB_HID_CONTROL_EXPERIMENT 0
#endif
#if A04_MUSB_HID_CONTROL_EXPERIMENT
#include "host_control.h"
#if !A04_MUSB_HOST_CORE_EXPERIMENT
#error A04_MUSB_HID_CONTROL_EXPERIMENT requires A04_MUSB_HOST_CORE_EXPERIMENT
#endif
#endif
#define REPORT_SLOTS 32u
typedef struct {
 WDFWAITLOCK lock; WDFQUEUE reads; UCHAR reports[REPORT_SLOTS][8];
 unsigned head,count,input_reports; BOOLEAN connected,overflow,draining; KB_SESSION session; KB_PORT port; A04_USB_WDF *usb; WDFWAITLOCK controller_lock; WDFTIMER poll; BOOLEAN board_owned, board_fault; A04_USB_BOARD board; A04_SHARED_USB_INTERFACE shared; BOOLEAN shared_valid;
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 WDFQUEUE controls; struct a04_control_host control; struct a04_control_request control_request;
 UCHAR control_data[8]; BOOLEAN control_ready,control_busy,control_poisoned,control_output; ULONGLONG control_deadline;
#endif
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
 if(!c->input_reports++)DbgPrint("MT8167 USB20 first keyboard input report received\n");
 slot=(c->head+c->count)%REPORT_SLOTS; RtlCopyMemory(c->reports[slot],report,8); ++c->count; c->connected=TRUE;
 WdfWaitLockRelease(c->lock); Drain(dev); return STATUS_SUCCESS;
}
/* On unplug queue a release report so a key cannot remain held. */
void KbHidDisconnected(WDFDEVICE dev)
{
 KB_HID_CONTEXT *c=KbHidContext(dev); static const UCHAR release[8]={0};
 WdfWaitLockAcquire(c->lock,NULL); c->head=0; c->count=0; c->input_reports=0; c->overflow=FALSE; c->connected=FALSE;
 RtlCopyMemory(c->reports[0],release,8); c->count=1;
 WdfWaitLockRelease(c->lock); WdfIoQueueStart(c->reads); Drain(dev);
}
/* Controller callbacks run at PASSIVE_LEVEL with external serialization.
 * They are never started by this HID filter without an owning board driver. */
static void SessionDelay(void *p,unsigned ms)
{ LARGE_INTEGER interval; UNREFERENCED_PARAMETER(p); interval.QuadPart=-10000LL*ms; (void)KeDelayExecutionThread(KernelMode,FALSE,&interval); }
static int SessionReport(void *p,const unsigned char *report)
{ return NT_SUCCESS(KbHidReport((WDFDEVICE)p,report)) ? 0 : -1; }
static void SessionRelease(void *p) { KbHidDisconnected((WDFDEVICE)p); }
#if A04_MUSB_HID_CONTROL_EXPERIMENT
/* Only retrieved requests may be touched. WDF owns/cancels queued requests;
 * EP0 always uses control_data, never the queued request's buffer. */
static void ControlComplete(KB_HID_CONTEXT *c,NTSTATUS status)
{
 WDFREQUEST request; PIRP irp; PHID_XFER_PACKET packet;
 if(!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->controls,&request)))return;
 if(NT_SUCCESS(status) && c->control_output)WdfRequestSetInformation(request,1);
 else if(NT_SUCCESS(status)) {
  irp=WdfRequestWdmGetIrp(request);packet=(PHID_XFER_PACKET)irp->UserBuffer;
  if(!packet || packet->reportId || KbCopyInputReport(packet->reportBuffer,packet->reportBufferLen,c->control_data))status=STATUS_INVALID_PARAMETER;
  else WdfRequestSetInformation(request,8);
 }
 WdfRequestComplete(request,status);
}
static int ControlReady(KB_HID_CONTEXT *c,const MT8167_USB20_CONTROLLER *controller)
{
 unsigned speed=c->session.speed==0xc0?A04_PORT_LOW_SPEED:(c->session.speed==0x40?A04_PORT_HIGH_SPEED:0);
 /* Called only after KbPort has flushed PIO, completed a bus reset and
  * re-enumerated. Old control requests have already been completed. */
 if(a04_control_init(&c->control,&controller->io) ||
    a04_port_power(&c->control.port,1,1) || a04_port_connect(&c->control.port,1) ||
    a04_port_reset_begin(&c->control.port) || a04_port_reset_done(&c->control.port,speed) || a04_queue_online(&c->control.queue))return -1;
 c->control_poisoned=FALSE;c->control_ready=TRUE;return 0;
}
static NTSTATUS ControlSubmit(WDFDEVICE dev,WDFREQUEST request,BOOLEAN output)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); PIRP irp;PHID_XFER_PACKET packet;NTSTATUS status;KB_CONTROL led_control;UCHAR led=0;struct a04_control_request *r=&c->control_request;
 irp=WdfRequestWdmGetIrp(request);packet=(PHID_XFER_PACKET)irp->UserBuffer;
 if(!packet || !packet->reportBuffer || packet->reportId || (!output && packet->reportBufferLen!=8 && packet->reportBufferLen!=9))return STATUS_INVALID_PARAMETER;
 WdfWaitLockAcquire(c->controller_lock,NULL);
 if(!c->board_owned || c->board_fault || !c->control_ready || c->control_busy || c->control_poisoned || c->session.enumeration.stage!=KB_READY) {WdfWaitLockRelease(c->controller_lock);return STATUS_DEVICE_NOT_READY;}
 if(output && KbOutputControl(&c->session.enumeration,packet->reportBuffer,packet->reportBufferLen,packet->reportId,&led_control,&led)) {
  WdfWaitLockRelease(c->controller_lock);return STATUS_INVALID_PARAMETER;
 }
 RtlZeroMemory(r,sizeof(*r));r->setup[0]=0xa1;r->setup[1]=1;r->setup[3]=1;
 r->setup[4]=(UCHAR)c->session.enumeration.interface_number;r->setup[5]=(UCHAR)(c->session.enumeration.interface_number>>8);r->setup[6]=8;
 if(output) {RtlCopyMemory(r->setup,led_control.setup,8);c->control_data[0]=led;}
 c->control_output=output;
 r->buffer=c->control_data;r->address=1;r->speed=c->session.speed;r->max_packet=c->session.enumeration.max_packet0;
 status=WdfRequestForwardToIoQueue(request,c->controls);
 if(!NT_SUCCESS(status)){WdfWaitLockRelease(c->controller_lock);return status;}
 c->control_busy=TRUE;c->control_deadline=KeQueryInterruptTime()+20000000ULL;
 if(a04_control_submit(&c->control,r) || a04_control_start(&c->control)) {
  c->control_poisoned=TRUE;c->control_ready=FALSE;c->control_busy=FALSE;ControlComplete(c,STATUS_DEVICE_HARDWARE_ERROR);
 }
 WdfWaitLockRelease(c->controller_lock);return STATUS_PENDING;
}
#endif
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
/* Mask without flushing our own work item, then stop the actual source.
 * The D0/release path subsequently drains queued DPC/work before unmapping. */
static void PortFault(WDFDEVICE dev,NTSTATUS status)
{
 KB_HID_CONTEXT*c=KbHidContext(dev);
 DbgPrint("MT8167 USB20 port fault state=%u enum=%u status=%08lx\n",(unsigned)c->port.stage,(unsigned)c->session.enumeration.stage,(ULONG)status);
 c->board_fault=TRUE;c->port.stage=KB_PORT_FAULT;
 c->session.enumeration.stage=KB_ERROR;c->port.rx_pending=FALSE;
 A04UsbWdfMask(c->usb);
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 if(c->control_busy) {
  (void)a04_control_cancel(&c->control,&c->control_request);
  c->control_poisoned=TRUE;c->control_busy=FALSE;ControlComplete(c,status);
 }
 c->control_ready=FALSE;
#else
 UNREFERENCED_PARAMETER(status);
#endif
 KbHidDisconnected(dev);
 if(c->board_owned){A04UsbHostStop(&c->board);c->board_owned=FALSE;}
}
static unsigned PortNowMs(void)
{
 ULONGLONG now=KeQueryInterruptTime();
 return KbPortMilliseconds((unsigned)(now>>32),(unsigned)now);
}
VOID KbPoll(WDFTIMER timer)
{
 WDFDEVICE dev=(WDFDEVICE)WdfTimerGetParentObject(timer); KB_HID_CONTEXT*c=KbHidContext(dev); ULONGLONG now;
 WdfWaitLockAcquire(c->controller_lock,NULL);
 if(c->board_owned && !c->board_fault && c->shared_valid) {
  if(c->shared.Health(c->shared.Header.Context))PortFault(dev,STATUS_DEVICE_POWER_FAILURE);
  else {
   now=KeQueryInterruptTime();
#if A04_MUSB_HID_CONTROL_EXPERIMENT
   if(c->control_busy && now>=c->control_deadline)PortFault(dev,STATUS_IO_TIMEOUT);
#endif
   if(!c->board_fault && KbPortTick(&c->port,KbPortMilliseconds((unsigned)(now>>32),(unsigned)now))<0)PortFault(dev,STATUS_IO_TIMEOUT);
  }
  if(c->board_owned)WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(10));
 }
 WdfWaitLockRelease(c->controller_lock);
}
static void KbControllerEvents(void *context,MT8167_USB20_EVENTS events)
{
 WDFDEVICE dev=(WDFDEVICE)context;KB_HID_CONTEXT*c=KbHidContext(dev);int result;
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 const MT8167_USB20_CONTROLLER *controller;
#endif
 WdfWaitLockAcquire(c->controller_lock,NULL);
 if(!c->board_owned || c->board_fault){WdfWaitLockRelease(c->controller_lock);return;}
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 if(events.usb&(0x10u|0x20u|0x80u)) {
  if(c->control_busy){
   (void)a04_control_cancel(&c->control,&c->control_request);
   c->control_poisoned=TRUE;c->control_busy=FALSE;ControlComplete(c,STATUS_DEVICE_NOT_CONNECTED);
  }
  c->control_ready=FALSE;
 } else if(c->control_busy && (events.tx&1u)) {
  struct a04_control_request *done=NULL;result=a04_control_events(&c->control,events,&done);
  if(result==1 && done){c->control_busy=FALSE;ControlComplete(c,done->transfer.actual==(c->control_output?1u:8u)?STATUS_SUCCESS:STATUS_DEVICE_DATA_ERROR);}
  else if(result<0 || c->control_request.transfer.state==A04_XFER_ABORTING)PortFault(dev,STATUS_DEVICE_HARDWARE_ERROR);
  events.tx=0;
 }
#endif
 if(!c->board_fault){
  result=KbPortEvents(&c->port,events,PortNowMs());
  if(result<0)PortFault(dev,STATUS_DEVICE_HARDWARE_ERROR);
  else if(result==1){
   DbgPrint("MT8167 USB20 keyboard enumerated vid=%04x pid=%04x speed=%02x\n",c->session.enumeration.vid,c->session.enumeration.pid,c->session.speed);
#if A04_MUSB_HID_CONTROL_EXPERIMENT
   controller=A04UsbWdfController(c->usb);if(!controller || ControlReady(c,controller))c->control_ready=FALSE;
#endif
  }
 }
 WdfWaitLockRelease(c->controller_lock);
}
static int BoardPower(void *p,int on)
{
 KB_HID_CONTEXT*c=KbHidContext((WDFDEVICE)p);int result;
 if(!on)return c->shared.Release(c->shared.Header.Context);
 result=c->shared.Acquire(c->shared.Header.Context);if(result)return result;
 /* Clocks are accessible now; reject active firmware DMA before PHY edits. */
 if(Mt8167UsbPioReady(A04UsbWdfController(c->usb))){(void)c->shared.Release(c->shared.Header.Context);return -1;}
 return 0;
}
static int BoardVbus(void *p,int on)
{ KB_HID_CONTEXT*c=KbHidContext((WDFDEVICE)p); return c->shared.SetVbus(c->shared.Header.Context,on); }
static unsigned char BoardPhyRead(void *p,unsigned off)
{ return A04UsbWdfPhyRead(KbHidContext((WDFDEVICE)p)->usb,off); }
static void BoardPhyWrite(void *p,unsigned off,unsigned char val)
{ A04UsbWdfPhyWrite(KbHidContext((WDFDEVICE)p)->usb,off,val); }
static unsigned BoardPhyRead32(void *p,unsigned off)
{ return A04UsbWdfPhyRead32(KbHidContext((WDFDEVICE)p)->usb,off); }
static unsigned char BoardMacRead(void *p,unsigned off)
{ const MT8167_USB20_CONTROLLER*c=A04UsbWdfController(KbHidContext((WDFDEVICE)p)->usb);return c->io.read8(c->io.context,off); }
static void BoardMacWrite(void *p,unsigned off,unsigned char val)
{ const MT8167_USB20_CONTROLLER*c=A04UsbWdfController(KbHidContext((WDFDEVICE)p)->usb);c->io.write8(c->io.context,off,val); }
NTSTATUS KbHidActivate(WDFDEVICE dev,const A04_USB_BOARD *board)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); NTSTATUS status;
 if(!board || !A04UsbWdfController(c->usb) || c->board_owned)return STATUS_INVALID_DEVICE_STATE;
 c->board=*board;
 WdfWaitLockAcquire(c->controller_lock,NULL);
 c->board_fault=FALSE;
 if(KbPortInit(&c->port,&c->session,&A04UsbWdfController(c->usb)->io,dev,SessionDelay,SessionReport,SessionRelease)){
  WdfWaitLockRelease(c->controller_lock);return STATUS_INVALID_PARAMETER;
 }
 if(A04UsbHostPrepare(&c->board)){WdfWaitLockRelease(c->controller_lock);return STATUS_DEVICE_POWER_FAILURE;}
 status=A04UsbWdfArm(c->usb);
 if(NT_SUCCESS(status) && A04UsbHostRun(&c->board))status=STATUS_DEVICE_POWER_FAILURE;
 if(NT_SUCCESS(status))c->board_owned=TRUE;
 WdfWaitLockRelease(c->controller_lock);
 /* Never flush work while holding controller_lock: the IRQ worker takes it. */
 if(!NT_SUCCESS(status)) { A04UsbWdfDisarm(c->usb); A04UsbHostStop(&c->board); }
 return status;
}
void KbHidDeactivate(WDFDEVICE dev)
{
 KB_HID_CONTEXT*c=KbHidContext(dev);
 WdfTimerStop(c->poll,TRUE); A04UsbWdfDisarm(c->usb);
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 WdfIoQueuePurgeSynchronously(c->controls);
#endif
 WdfWaitLockAcquire(c->controller_lock,NULL);
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 if(c->control_busy)c->control_poisoned=TRUE;
 c->control_busy=FALSE;c->control_ready=FALSE;
#endif
 c->session.enumeration.stage=KB_ERROR; c->port.rx_pending=FALSE;
 KbHidDisconnected(dev);
 if(c->board_owned) { A04UsbHostStop(&c->board); c->board_owned=FALSE; }
 WdfWaitLockRelease(c->controller_lock);
}
NTSTATUS KbPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 KB_HID_CONTEXT*c=KbHidContext(dev); NTSTATUS status;
 status=A04UsbWdfPrepare(c->usb,raw,translated); if(!NT_SUCCESS(status))return status;
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
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 WdfIoQueueStart(c->controls);
#endif
 RtlZeroMemory(&b,sizeof(b)); b.context=dev; b.power=BoardPower; b.vbus=BoardVbus;
 b.phy_read=BoardPhyRead; b.phy_write=BoardPhyWrite; b.mac_read=BoardMacRead; b.mac_write=BoardMacWrite; b.delay_ms=SessionDelay;b.phy_read32=BoardPhyRead32;
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
static const unsigned char *ReportDescriptor(unsigned *length)
{
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 return KbHidOutputDescriptor(length);
#else
 return KbHidDescriptor(length);
#endif
}
VOID KbHidIoctl(WDFQUEUE queue,WDFREQUEST request,size_t outlen,size_t inlen,ULONG code)
{
 WDFDEVICE dev=WdfIoQueueGetDevice(queue); KB_HID_CONTEXT *c=KbHidContext(dev);
 HID_DESCRIPTOR h={0}; HID_DEVICE_ATTRIBUTES attr={0}; const unsigned char *desc; unsigned len; NTSTATUS status=STATUS_NOT_SUPPORTED;
 UNREFERENCED_PARAMETER(outlen); UNREFERENCED_PARAMETER(inlen);
 switch(code) {
 case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
  (void)ReportDescriptor(&len); h.bLength=sizeof(h); h.bDescriptorType=HID_HID_DESCRIPTOR_TYPE;
  h.bcdHID=0x0111; h.bNumDescriptors=1; h.DescriptorList[0].bReportType=HID_REPORT_DESCRIPTOR_TYPE;
  h.DescriptorList[0].wReportLength=(USHORT)len; status=CopyOutput(request,&h,sizeof(h)); break;
 case IOCTL_HID_GET_REPORT_DESCRIPTOR: desc=ReportDescriptor(&len); status=CopyOutput(request,desc,len); break;
 case IOCTL_HID_GET_DEVICE_ATTRIBUTES: attr.Size=sizeof(attr); attr.VersionNumber=1; status=CopyOutput(request,&attr,sizeof(attr)); break;
 case IOCTL_HID_READ_REPORT:
  status=WdfRequestForwardToIoQueue(request,c->reads); if(NT_SUCCESS(status)) { Drain(dev); return; } break;
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 case IOCTL_HID_GET_INPUT_REPORT:
  status=ControlSubmit(dev,request,FALSE);if(status==STATUS_PENDING)return;break;
 case IOCTL_HID_WRITE_REPORT:
 case IOCTL_HID_SET_OUTPUT_REPORT:
  status=ControlSubmit(dev,request,TRUE);if(status==STATUS_PENDING)return;break;
#endif
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
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual); s=WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads);if(!NT_SUCCESS(s))return s;
#if A04_MUSB_HID_CONTROL_EXPERIMENT
 WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual);return WdfIoQueueCreate(dev,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->controls);
#else
 return STATUS_SUCCESS;
#endif
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{ WDF_DRIVER_CONFIG c; WDF_DRIVER_CONFIG_INIT(&c,KbHidAdd); return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE); }

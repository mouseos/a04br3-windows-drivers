/* MT8167 USB20 controller ownership is separate from the HID report shim.
 * All passive operations and session callbacks are serialized by the caller. */
#include "wdf_controller.h"
#include "pnp_resources.h"
#define USB_SIZE 0x10000u
struct A04_USB_WDF {
 WDFINTERRUPT irq; WDFWORKITEM work; volatile UCHAR *mac,*phy;
 volatile LONG armed,pending_usb,pending_tx,pending_rx,worker_busy;
 A04_USB_EVENT_CALLBACK callback; void *callback_context;
 MT8167_USB20_CONTROLLER controller;
};
typedef struct { A04_USB_WDF *owner; } USB_WORK_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_USB_WDF,UsbIrqContext)
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(USB_WORK_CONTEXT,UsbWorkContext)
EVT_WDF_INTERRUPT_ISR UsbIsr;
EVT_WDF_INTERRUPT_DPC UsbDpc;
EVT_WDF_WORKITEM UsbWork;
static unsigned char Rd8(void*p,unsigned o) { A04_USB_WDF*c=p; return READ_REGISTER_UCHAR(c->mac+o); }
static unsigned short Rd16(void*p,unsigned o) { A04_USB_WDF*c=p; return READ_REGISTER_USHORT((volatile USHORT *)(c->mac+o)); }
static unsigned Rd32(void*p,unsigned o) { A04_USB_WDF*c=p; return READ_REGISTER_ULONG((volatile ULONG *)(c->mac+o)); }
static void Wr8(void*p,unsigned o,unsigned char v) { A04_USB_WDF*c=p; WRITE_REGISTER_UCHAR(c->mac+o,v); }
static void Wr16(void*p,unsigned o,unsigned short v) { A04_USB_WDF*c=p; WRITE_REGISTER_USHORT((volatile USHORT *)(c->mac+o),v); }
static void Wr32(void*p,unsigned o,unsigned v) { A04_USB_WDF*c=p; WRITE_REGISTER_ULONG((volatile ULONG *)(c->mac+o),v); }
BOOLEAN UsbIsr(WDFINTERRUPT irq,ULONG message)
{
 A04_USB_WDF*c=UsbIrqContext(irq); MT8167_USB20_EVENTS e; int n;
 UNREFERENCED_PARAMETER(message);
 if(!InterlockedCompareExchange(&c->armed,0,0))return FALSE;
 n=Mt8167UsbInterruptCapture(&c->controller,&e); if(n!=1)return FALSE;
 InterlockedOr(&c->pending_usb,(LONG)e.usb); InterlockedOr(&c->pending_tx,(LONG)e.tx); InterlockedOr(&c->pending_rx,(LONG)e.rx);
 WdfInterruptQueueDpcForIsr(irq); return TRUE;
}
VOID UsbDpc(WDFINTERRUPT irq,WDFOBJECT device)
{ A04_USB_WDF*c=UsbIrqContext(irq); UNREFERENCED_PARAMETER(device); if(c->work)WdfWorkItemEnqueue(c->work); }
VOID UsbWork(WDFWORKITEM item)
{
 A04_USB_WDF*c=UsbWorkContext(item)->owner; MT8167_USB20_EVENTS e;
 if(InterlockedCompareExchange(&c->worker_busy,1,0))return;
 for(;;) {
  e.usb=(unsigned)InterlockedExchange(&c->pending_usb,0); e.tx=(unsigned)InterlockedExchange(&c->pending_tx,0); e.rx=(unsigned)InterlockedExchange(&c->pending_rx,0);
  if(!(e.usb|e.tx|e.rx))break;
  if(InterlockedCompareExchange(&c->armed,0,0) && c->callback)c->callback(c->callback_context,e);
 }
 InterlockedExchange(&c->worker_busy,0);
 if(InterlockedCompareExchange(&c->armed,0,0) && (c->pending_usb||c->pending_tx||c->pending_rx))WdfWorkItemEnqueue(item);
}
NTSTATUS A04UsbWdfCreate(WDFDEVICE device,A04_USB_EVENT_CALLBACK callback,void *context,A04_USB_WDF **out)
{
 WDF_INTERRUPT_CONFIG ic; WDF_WORKITEM_CONFIG wc; WDF_OBJECT_ATTRIBUTES a; WDFINTERRUPT irq; A04_USB_WDF*c; NTSTATUS s;
 if(!device || !callback || !out)return STATUS_INVALID_PARAMETER;
 WDF_INTERRUPT_CONFIG_INIT(&ic,UsbIsr,UsbDpc); ic.AutomaticSerialization=FALSE;
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,A04_USB_WDF); s=WdfInterruptCreate(device,&ic,&a,&irq); if(!NT_SUCCESS(s))return s;
 c=UsbIrqContext(irq); RtlZeroMemory(c,sizeof(*c)); c->irq=irq; c->callback=callback; c->callback_context=context;
 c->controller.io.context=c; c->controller.io.read8=Rd8; c->controller.io.read16=Rd16; c->controller.io.write8=Wr8; c->controller.io.write16=Wr16; c->controller.read32=Rd32; c->controller.write32=Wr32;
 WDF_WORKITEM_CONFIG_INIT(&wc,UsbWork); wc.AutomaticSerialization=FALSE;
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,USB_WORK_CONTEXT); a.ParentObject=irq; a.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfWorkItemCreate(&wc,&a,&c->work); if(!NT_SUCCESS(s)){WdfObjectDelete(irq);return s;}
 UsbWorkContext(c->work)->owner=c; *out=c; return STATUS_SUCCESS;
}
NTSTATUS A04UsbWdfPrepare(A04_USB_WDF*c,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 PHYSICAL_ADDRESS mac,phy;struct a04_usb_memory_pair mem[2];uint64_t mac64,phy64;
 ULONG i,n=0,interrupts=0;NTSTATUS s=STATUS_DEVICE_CONFIGURATION_ERROR;
 if(!c || c->mac || c->phy)return STATUS_INVALID_DEVICE_STATE;
 if(!raw || !translated || WdfCmResourceListGetCount(raw)!=3 ||
    WdfCmResourceListGetCount(translated)!=3)return s;
 for(i=0;i<3;++i){
  PCM_PARTIAL_RESOURCE_DESCRIPTOR r=WdfCmResourceListGetDescriptor(raw,i);
  PCM_PARTIAL_RESOURCE_DESCRIPTOR t=WdfCmResourceListGetDescriptor(translated,i);
  if(!r || !t || r->Type!=t->Type || r->ShareDisposition!=CmResourceShareDeviceExclusive ||
     t->ShareDisposition!=CmResourceShareDeviceExclusive)return s;
  if(r->Type==CmResourceTypeInterrupt){
   if(++interrupts!=1 || (r->Flags&(CM_RESOURCE_INTERRUPT_LATCHED|CM_RESOURCE_INTERRUPT_MESSAGE)) ||
      (t->Flags&(CM_RESOURCE_INTERRUPT_LATCHED|CM_RESOURCE_INTERRUPT_MESSAGE)) ||
      r->u.Interrupt.Vector!=104 || !t->u.Interrupt.Vector || !t->u.Interrupt.Affinity)return s;
  } else if(r->Type==CmResourceTypeMemory){
   if(n==2 || r->u.Memory.Start.QuadPart<0 || t->u.Memory.Start.QuadPart<0 ||
      r->u.Memory.Length!=t->u.Memory.Length)return s;
   mem[n].raw=(uint64_t)r->u.Memory.Start.QuadPart;
   mem[n].translated=(uint64_t)t->u.Memory.Start.QuadPart;mem[n].length=r->u.Memory.Length;++n;
  } else return s;
 }
 if(n!=2 || interrupts!=1 || a04_usb_translate_memory(mem,&mac64,&phy64))return s;
 mac.QuadPart=(LONGLONG)mac64;phy.QuadPart=(LONGLONG)phy64;
 c->mac=MmMapIoSpace(mac,USB_SIZE,MmNonCached); if(!c->mac)return STATUS_INSUFFICIENT_RESOURCES;
 c->phy=MmMapIoSpace(phy,USB_SIZE,MmNonCached);
 if(!c->phy){MmUnmapIoSpace((PVOID)c->mac,USB_SIZE);c->mac=NULL;return STATUS_INSUFFICIENT_RESOURCES;}
 return STATUS_SUCCESS;
}
const MT8167_USB20_CONTROLLER *A04UsbWdfController(A04_USB_WDF*c)
{ return c && c->mac && c->phy ? &c->controller : NULL; }
static BOOLEAN DisarmSynchronized(WDFINTERRUPT irq,WDFCONTEXT context)
{
 A04_USB_WDF*c=context; UNREFERENCED_PARAMETER(irq);
 if(InterlockedExchange(&c->armed,0))Mt8167UsbInterruptDisable(&c->controller);
 return TRUE;
}
unsigned char A04UsbWdfPhyRead(A04_USB_WDF*c,unsigned off)
{ return READ_REGISTER_UCHAR(c->phy+0x800u+off); }
void A04UsbWdfPhyWrite(A04_USB_WDF*c,unsigned off,unsigned char value)
{ WRITE_REGISTER_UCHAR(c->phy+0x800u+off,value); }
static BOOLEAN ArmSynchronized(WDFINTERRUPT irq,WDFCONTEXT context)
{
 A04_USB_WDF*c=context;UNREFERENCED_PARAMETER(irq);
 if(InterlockedCompareExchange(&c->armed,1,0))return FALSE;
 if(Mt8167UsbInterruptEnable(&c->controller)) {
  Mt8167UsbInterruptDisable(&c->controller);InterlockedExchange(&c->armed,0);return FALSE;
 }
 return TRUE;
}
NTSTATUS A04UsbWdfArm(A04_USB_WDF*c)
{
 if(!c || !c->mac || !c->phy || InterlockedCompareExchange(&c->armed,0,0))return STATUS_INVALID_DEVICE_STATE;
 /* Linux MT8167 USB20 platform declares an 8 KiB dynamic FIFO. */
 if(Mt8167UsbFifoInit(&c->controller,8192))return STATUS_DEVICE_HARDWARE_ERROR;
 if(!WdfInterruptSynchronize(c->irq,ArmSynchronized,c))return STATUS_DEVICE_HARDWARE_ERROR;
 return STATUS_SUCCESS;
}
void A04UsbWdfDisarm(A04_USB_WDF*c)
{
 if(!c)return;
 if(InterlockedCompareExchange(&c->armed,0,0))
  (void)WdfInterruptSynchronize(c->irq,DisarmSynchronized,c);
 KeFlushQueuedDpcs(); WdfWorkItemFlush(c->work);
 InterlockedExchange(&c->pending_usb,0); InterlockedExchange(&c->pending_tx,0); InterlockedExchange(&c->pending_rx,0);
}
void A04UsbWdfRelease(A04_USB_WDF*c)
{
 if(!c)return; A04UsbWdfDisarm(c);
 if(c->phy){MmUnmapIoSpace((PVOID)c->phy,USB_SIZE);c->phy=NULL;}
 if(c->mac){MmUnmapIoSpace((PVOID)c->mac,USB_SIZE);c->mac=NULL;}
}

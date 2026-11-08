/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Same pwrapLock and usb_owner as the v1 clients; no second power owner. */
static unsigned MusbGpio(void *p,unsigned o){return ParentUsbGpioRead(p,o);}
static void MusbGpioWrite(void *p,unsigned o,unsigned v){ParentUsbGpioWrite(p,o,v);}
static unsigned MusbMac(void *p,unsigned o,unsigned width)
{volatile UCHAR *b=((A04_PARENT_CONTEXT *)p)->musb.mac;if(width==1)return READ_REGISTER_UCHAR(b+o);if(width==2)return READ_REGISTER_USHORT((volatile USHORT *)(b+o));return READ_REGISTER_ULONG((volatile ULONG *)(b+o));}
static void MusbMacWrite(void *p,unsigned o,unsigned width,unsigned v)
{volatile UCHAR *b=((A04_PARENT_CONTEXT *)p)->musb.mac;if(width==1)WRITE_REGISTER_UCHAR(b+o,(UCHAR)v);else if(width==2)WRITE_REGISTER_USHORT((volatile USHORT *)(b+o),(USHORT)v);else WRITE_REGISTER_ULONG((volatile ULONG *)(b+o),v);}
static unsigned MusbPhy(void *p,unsigned o){return READ_REGISTER_UCHAR((volatile UCHAR *)((A04_PARENT_CONTEXT *)p)->musb.phy+0x800+o);}
static void MusbPhyWrite(void *p,unsigned o,unsigned v){WRITE_REGISTER_UCHAR((volatile UCHAR *)((A04_PARENT_CONTEXT *)p)->musb.phy+0x800+o,(UCHAR)v);}
static void MusbDelay(void *p,unsigned ms){LARGE_INTEGER t;UNREFERENCED_PARAMETER(p);t.QuadPart=-(LONGLONG)ms*10000;KeDelayExecutionThread(KernelMode,FALSE,&t);}
static struct mt_musb_board_io MusbIo(A04_PARENT_CONTEXT *c)
{struct mt_musb_board_io i={c,MusbGpio,MusbGpioWrite,MusbMac,MusbMacWrite,MusbPhy,MusbPhyWrite,MusbDelay};return i;}
static void MusbSnapshot(A04_PARENT_CONTEXT *c,MT_MUSB_OBSERVATION *o)
{
 struct mt_musb_board_sample s;struct mt_musb_board_io i=MusbIo(c);RtlZeroMemory(o,sizeof(*o));
 o->bytes=sizeof(*o);o->generation=c->musb.generation;o->observed_100ns=KeQueryInterruptTime();
 o->capabilities=MT_MUSB_REQUIRED;o->raw_mac.QuadPart=0x11100000;o->raw_phy.QuadPart=0x11110000;
 o->mac=c->musb.mac_pa;o->phy=c->musb.phy_pa;o->mac_bytes=o->phy_bytes=0x10000;
 if(c->musb.bound&&c->musb.accessible)o->valid|=MT_MUSB_RESOURCE_ASSIGNED|MT_MUSB_STOP_CAPABLE;
 if(!mt_musb_pin_candidate(&i,&s)){
  o->gpio21=s.oc;o->gpio22=s.source;o->gpio41=s.id;o->gpio34=s.detect;
  o->host_role=!s.id;o->valid|=MT_MUSB_ROLE;
  if(s.oc_valid){o->valid|=MT_MUSB_OC;o->oc_clear=s.oc;o->evidence|=MT_MUSB_OC_PIN21;}
  o->source_off=!s.source;if(o->source_off)o->evidence|=MT_MUSB_SOURCE_OFF;
  if(!s.id&&s.detect&&!s.source)o->evidence|=MT_MUSB_QUERY_READY;
  if(s.id)o->reason=MT_MUSB_WAIT_ROLE;else if(!s.detect&&!c->musb.powered)o->reason=MT_MUSB_WAIT_PC;
 }
 if(!c->usb.leased||c->musb.held)o->valid|=MT_MUSB_EXCLUSIVE;
 if(c->musb.mac&&c->musb.phy&&c->usb.leased&&c->musb.accessible){
  o->devctl=MusbMac(c,0x60,1);o->intrusb=MusbMac(c,0xa,1);
  if(!mt_musb_unforced(&i)){
   o->valid|=MT_MUSB_VBUS;o->vbus_on=(o->devctl&0x18)==0x18;
   o->evidence|=MT_MUSB_NO_FORCED_VBUS|MT_MUSB_VBUS_COMPARATOR;
  }
  if(!mt_musb_engines_idle(&i))o->valid|=MT_MUSB_DMA_OFF;
  if(!mt_musb_irq_off(&i))o->valid|=MT_MUSB_IRQ_OFF;
  if(c->musb.stop_ok)o->valid|=MT_MUSB_STOPPED;
 }
}
static int MusbStopLocked(A04_PARENT_CONTEXT *c)
{
 struct mt_musb_board_io i=MusbIo(c);struct mt_musb_board_sample s;unsigned n;
 c->musb.stop_ok=0;
 if(!c->musb.held)return 0;
 if(!c->musb.accessible||!c->gpio||!c->musb.mac||!c->musb.phy)return -1;
 /* Remove our source even if engines are unknown. No DMA control/address
  * writes or unproved reset/fabric-drain claims are made. */
 if(a04_usb_owner_vbus_locked(&c->usb,0))return -1;c->musb.powered=0;
 if(mt_musb_stop_pio(&i)||mt_musb_release_forces(&i))return -1;
 for(n=0;n<50;n++){
  if(mt_musb_pin_candidate(&i,&s)||s.source)return -1;
  /* Zero comparator band is real sensed session-end. An externally powered
   * PC cable is separately identified; source latch OFF is not called 0 V. */
  if((MusbMac(c,0x60,1)&0x18)==0||(s.id&&!s.detect))break;
  MusbDelay(c,10);
 }
 if(n==50||mt_musb_engines_idle(&i)||mt_musb_irq_off(&i))return -1;
 MusbMacWrite(c,0xa,1,0xb0);MusbMacWrite(c,2,2,0xffff);MusbMacWrite(c,4,2,0xffff);
 c->musb.stop_ok=1;return 0;
}
static NTSTATUS ParentMusbQuery(PVOID p,MT_MUSB_OBSERVATION *o)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return STATUS_DEVICE_NOT_READY;
 MusbSnapshot(c,o);
 if(c->musb.bound&&c->musb.accessible&&c->usb.online&&!c->usb.leased&&!c->musb.poisoned&&
    (o->evidence&MT_MUSB_QUERY_READY)&&o->generation)status=STATUS_SUCCESS;
 ParentUnlock(c);return status;
}
static NTSTATUS ParentMusbAcquire(PVOID p,ULONGLONG generation)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);MT_MUSB_OBSERVATION o;struct mt_musb_board_io i;NTSTATUS result=STATUS_DEVICE_NOT_READY;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return result;
 MusbSnapshot(c,&o);
 if(!c->musb.bound||!c->musb.accessible||c->musb.poisoned||c->usb.leased||generation!=c->musb.generation||
    !(o.evidence&MT_MUSB_QUERY_READY)||!c->usb.online)goto done;
 if(a04_usb_owner_acquire_locked(&c->usb))goto done;
 c->musb.held=1;c->musb.stop_ok=0;i=MusbIo(c);
 c->musb.mac=MmMapIoSpace(c->musb.mac_pa,0x10000,MmNonCached);
 c->musb.phy=MmMapIoSpace(c->musb.phy_pa,0x10000,MmNonCached);
 if(!c->musb.mac||!c->musb.phy||mt_musb_oc_prepare(&i))goto fault;
 /* Assigned resources, clock/rail readback and a live HWVERS distinguish a
  * readable controller from a fabricated all-zero MMIO observation. */
 {unsigned v=MusbMac(c,0x6c,2);if(!v||v==0xffff||v!=MusbMac(c,0x6c,2))goto fault;}
 if(MusbStopLocked(c))goto fault;
 result=STATUS_SUCCESS;goto done;
fault:
 c->musb.poisoned=1;c->usb.poisoned=1;
 if(c->gpio)(void)A04Gpio22Vbus(&c->usb.gpio,0);
 /* Partial ownership is retained. ParentUnmap must reject it; the caller's
  * failed Acquire has no URB/shadow or DMA backing to release. */
 result=STATUS_DEVICE_HARDWARE_ERROR;
done:ParentUnlock(c);return result;
}
static NTSTATUS ParentMusbObserve(PVOID p,MT_MUSB_OBSERVATION *o)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return status;
 if(c->musb.held&&c->musb.accessible){MusbSnapshot(c,o);status=STATUS_SUCCESS;}
 ParentUnlock(c);return status;
}
static NTSTATUS ParentMusbVbus(PVOID p,BOOLEAN on)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);MT_MUSB_OBSERVATION o;struct mt_musb_board_io i;unsigned n,stable=0;NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||ParentLock(c))return status;
 if(!c->musb.held||!c->musb.accessible||!c->musb.mac||!c->musb.phy)goto done;
 i=MusbIo(c);MusbSnapshot(c,&o);
 if(on&&c->musb.powered){
  if(o.host_role&&o.oc_clear&&(o.evidence&MT_MUSB_NO_FORCED_VBUS)&&o.vbus_on)status=STATUS_SUCCESS;
  else goto fault;goto done;
 }
 if(!on){status=a04_usb_owner_vbus_locked(&c->usb,0)?STATUS_DEVICE_HARDWARE_ERROR:STATUS_SUCCESS;c->musb.powered=0;goto done;}
 if(c->musb.poisoned||!o.host_role||!o.oc_clear||!(o.valid&MT_MUSB_OC)||
    (o.evidence&(MT_MUSB_NO_FORCED_VBUS|MT_MUSB_QUERY_READY))!=(MT_MUSB_NO_FORCED_VBUS|MT_MUSB_QUERY_READY)||
    (o.devctl&0x18)||mt_musb_engines_idle(&i))goto done;
 c->musb.stop_ok=0;
 if(a04_usb_owner_vbus_locked(&c->usb,1))goto fault;
 for(n=0;n<50;n++){
  MusbSnapshot(c,&o);
  if(!o.host_role||!o.oc_clear||!(o.valid&MT_MUSB_OC)||(o.intrusb&0x80)||!(o.evidence&MT_MUSB_NO_FORCED_VBUS))goto fault;
  stable=o.vbus_on?stable+1:0;if(stable==5){c->musb.powered=1;status=STATUS_SUCCESS;goto done;}
  MusbDelay(c,2);
 }
fault:
 (void)a04_usb_owner_vbus_locked(&c->usb,0);c->musb.powered=0;c->musb.poisoned=1;status=STATUS_DEVICE_POWER_FAILURE;
done:ParentUnlock(c);return status;
}
static NTSTATUS ParentMusbStopAndProve(PVOID p,MT_MUSB_OBSERVATION *o)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status=STATUS_DEVICE_HARDWARE_ERROR;
 if(!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return status;
 /* Stop must still run after global admission closes. The retained lease,
  * accessibility and same parent mutex govern this cleanup-only operation. */
 ExAcquireFastMutex(&c->pwrapLock);
 if(!c->removed&&c->musb.held&&!MusbStopLocked(c)){MusbSnapshot(c,o);status=STATUS_SUCCESS;}
 else c->musb.poisoned=1;
 ExReleaseFastMutex(&c->pwrapLock);return status;
}
static NTSTATUS ParentMusbRelease(PVOID p)
{
 A04_PARENT_CONTEXT *c=ParentContext((WDFDEVICE)p);NTSTATUS status=STATUS_DEVICE_HARDWARE_ERROR;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return status;
 ExAcquireFastMutex(&c->pwrapLock);
 if(c->musb.held&&c->musb.stop_ok&&!c->removed&&!MusbStopLocked(c)&&!a04_usb_owner_release_locked(&c->usb)){
  c->musb.held=0;if(c->musb.mac)MmUnmapIoSpace(c->musb.mac,0x10000);if(c->musb.phy)MmUnmapIoSpace(c->musb.phy,0x10000);
  c->musb.mac=c->musb.phy=NULL;status=STATUS_SUCCESS;
 }
 ExReleaseFastMutex(&c->pwrapLock);return status;
}

/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
static ULONGLONG hcd_now=10000000;
#define KeQueryInterruptTime() hcd_now
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static NTSTATUS admission=STATUS_NOT_SUPPORTED;static ULONG opens,cancels,closes,completed,done,fault;
static ULONG root_kind,root_result,connected;static struct a04_hub_observation observation;
static struct a04_mp_parameters *completed_parameters;
static ULONG complete_cookie,completion_status;
static ULONG async_count,invalidations,epoch_copy,root_invalidations;
static VOID InvalidateRoot(PVOID h){(void)h;root_invalidations++;}
static NTSTATUS Schedule(PVOID h,ULONG ms,PVOID payload,ULONG bytes,A04_PORT_ASYNC_ROUTINE routine)
{(void)h;(void)ms;(void)bytes;(void)routine;async_count++;epoch_copy=*(ULONG *)payload;return STATUS_SUCCESS;}
static VOID InvalidateController(PVOID h,ULONG reason){(void)h;if(reason==3)invalidations++;}
NTSTATUS MtMusbOpen(PDEVICE_OBJECT f,struct mt_musb_runtime **out)
{(void)f;opens++;*out=NT_SUCCESS(admission)?(struct mt_musb_runtime *)1:NULL;return admission;}
BOOLEAN MtMusbClose(struct mt_musb_runtime *r){if(r)closes++;return !fault;}
ULONG MtMusbSubmit(struct mt_musb_runtime *r,ULONG c,const struct a04_mp_properties *e,ULONG t,const struct a04_mp_parameters *p,PVOID b)
{(void)r;(void)c;(void)e;(void)t;(void)p;(void)b;done=0;return 0;}
ULONG MtMusbSubmitDeadline(struct mt_musb_runtime *r,ULONG c,const struct a04_mp_properties *e,ULONG t,
 const struct a04_mp_parameters *p,PVOID b,ULONGLONG deadline)
{(void)deadline;return MtMusbSubmit(r,c,e,t,p,b);}
VOID MtMusbCancel(struct mt_musb_runtime *r,ULONG c){(void)r;(void)c;cancels++;done=0;}
ULONG MtMusbTake(struct mt_musb_runtime *r,ULONG c,PVOID b,ULONG n,PULONG actual,PULONG status,PULONG toggle)
{(void)r;(void)c;(void)b;(void)n;if(!done||(complete_cookie&&c!=complete_cookie))return 1;done=0;*actual=n<3?n:3;*status=completion_status;*toggle=1;return 0;}
ULONG MtMusbRecoverToggle(struct mt_musb_runtime *r,ULONG c,PULONG t){(void)r;(void)c;*t=1;return 0;}
ULONG MtMusbRootQueue(struct mt_musb_runtime *r,ULONG kind){(void)r;root_kind=kind;return 0;}
ULONG MtMusbObserve(struct mt_musb_runtime *r,struct a04_hub_observation *o,PULONG c,PULONG kind,PULONG result)
{(void)r;*o=observation;*c=connected;*kind=root_kind;*result=root_result;root_kind=0;return fault;}
ULONG MtMusbFrame(struct mt_musb_runtime *r){(void)r;return 10;}
static VOID Complete(PVOID h,PVOID e,struct a04_mp_parameters *p,USBD_STATUS s,ULONG actual)
{(void)e;(void)s;(void)actual;completed++;completed_parameters=p;Packet.check(h); /* reentrant callback */}
int main(void)
{
 static struct a04_hcd h;struct a04_mp_resources r={0};struct a04_mp_endpoint ep;
 struct a04_mp_properties e={0};struct a04_mp_parameters p={0};struct a04_mp_transfer t;
 struct a04_mp_sg sg={0};ULONG actual,status;UCHAR data[8];
 r.fdo=(PDEVICE_OBJECT)1;r.pdo=(PDEVICE_OBJECT)2;FillPacket();Packet.complete=Complete;Packet.async_callback=Schedule;Packet.invalidate_controller=InvalidateController;
 CHECK(Packet.start(&h,&r)==A04_MP_ERROR&&opens==0);Packet.invalidate_root=InvalidateRoot;
 CHECK(Packet.start(&h,&r)==A04_MP_UNSUPPORTED&&!h.started&&!h.runtime&&opens==1);
 admission=STATUS_SUCCESS;CHECK(!Packet.start(&h,&r));CHECK(!Packet.port_power(&h,1));
 CHECK(!(h.port.status&A04_PORT_POWER)); /* accepting command is not proof */
 observation.qualified=observation.id_host=observation.overcurrent_clear=observation.vbus_verified=observation.endpoints_quiesced=1;
 connected=1;Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_CONNECTION);
 CHECK(!(h.port.status&A04_PORT_ENABLE));CHECK(!Packet.port_reset(&h,1));
 observation.reset_asserted_ms=50;Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_ENABLE);
 Packet.root_enable(&h);Packet.poll_controller(&h);CHECK(root_invalidations==1);
 CHECK(!Packet.clear_connect_change(&h,1)&&!Packet.clear_enable_change(&h,1)&&!Packet.clear_reset_change(&h,1));
 Packet.poll_controller(&h);CHECK(root_invalidations==1);
 /* Native endpoints through an external HS/FS hub have no TT context.
  * A finite TT address is always rejected, including control enumeration. */
 e.address=2;e.endpoint=0;e.type=1;e.packet=64;e.speed=2;e.hub=0xffff;e.port=7;
 CHECK(!Packet.open(&h,&e,&ep));Packet.close(&h,&ep,FALSE);
 e.hub=3;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.speed=1;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);e.hub=0xffff;
 e.type=3;e.endpoint=0x81;e.direction=0;e.packet=8;e.speed=2;
 for(unsigned period=1;period<=32;period<<=1){e.period=(UCHAR)period;CHECK(!Packet.open(&h,&e,&ep));Packet.close(&h,&ep,FALSE);}
 e.period=3;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);e.hub=0;e.port=0;
 /* HS bulk is admitted only at 512; unsupported profiles reject before
  * constructing an endpoint. USBPORT callback contracts are unchanged. */
 e.address=1;e.endpoint=1;e.type=2;e.direction=1;e.speed=2;e.packet=512;
 CHECK(!Packet.open(&h,&e,&ep));Packet.close(&h,&ep,FALSE);
 e.packet=64;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.packet=513;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.packet=512;e.speed=1;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.packet=12;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.packet=512;e.speed=2;e.type=3;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);
 e.type=2;e.hub=1;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);e.hub=0;
 e.transactions=2;CHECK(Packet.open(&h,&e,&ep)==A04_MP_UNSUPPORTED);e.transactions=0;
 e.address=1;e.endpoint=0x81;e.type=3;e.packet=8;e.speed=1;e.period=1;e.direction=0;
 CHECK(!Packet.open(&h,&e,&ep));Packet.set_state(&h,&ep,A04_EP_ACTIVE);p.bytes=8;sg.mapped_va=data;
 CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));done=1;Packet.poll_endpoint(&h,&ep);
 CHECK(completed==1&&completed_parameters==&p&&t.actual==3&&ep.toggle==1);
 CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));Packet.abort(&h,&ep,&t,&actual);
 CHECK(cancels==1&&!AnyActive(&h)&&actual==0&&completed==1&&ep.toggle_unknown);
 /* Caller can reuse every USBPORT object immediately after cancel/close. */
 RtlZeroMemory(&p,sizeof(p));RtlZeroMemory(&t,sizeof(t));Packet.poll_endpoint(&h,&ep);CHECK(completed==1);
 p.bytes=8;CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));Packet.close(&h,&ep,FALSE);
 CHECK(cancels==2&&!AnyActive(&h)&&ep.state==A04_EP_CLOSED);
 {ULONG prior=async_count;ULONG wrong=epoch_copy+1;
 AsyncPoll(&h,&wrong);CHECK(async_count==prior&&!invalidations);
 AsyncPoll(&h,&epoch_copy);CHECK(async_count==prior+1&&invalidations==1);}
 fault=1;Packet.stop(&h,FALSE);CHECK(closes==1&&!h.runtime&&!h.started&&h.fault);
  {ULONG prior=async_count;AsyncPoll(&h,&epoch_copy);CHECK(async_count==prior);}
 return 0;
}

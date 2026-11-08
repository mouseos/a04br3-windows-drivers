/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static NTSTATUS admission=STATUS_NOT_SUPPORTED;static ULONG opens,cancels,closes,completed,done,fault;
static ULONG root_kind,root_result,connected;static struct a04_hub_observation observation;
static struct a04_mp_parameters *completed_parameters;
static ULONG async_count,invalidations,epoch_copy;
static NTSTATUS Schedule(PVOID h,ULONG ms,PVOID payload,ULONG bytes,A04_PORT_ASYNC_ROUTINE routine)
{(void)h;(void)ms;(void)bytes;(void)routine;async_count++;epoch_copy=*(ULONG *)payload;return STATUS_SUCCESS;}
static VOID InvalidateController(PVOID h,ULONG reason){(void)h;if(reason==3)invalidations++;}
NTSTATUS MtMusbOpen(PDEVICE_OBJECT f,struct mt_musb_runtime **out)
{(void)f;opens++;*out=NT_SUCCESS(admission)?(struct mt_musb_runtime *)1:NULL;return admission;}
BOOLEAN MtMusbClose(struct mt_musb_runtime *r){if(r)closes++;return !fault;}
ULONG MtMusbSubmit(struct mt_musb_runtime *r,ULONG c,const struct a04_mp_properties *e,ULONG t,const struct a04_mp_parameters *p,PVOID b)
{(void)r;(void)c;(void)e;(void)t;(void)p;(void)b;done=0;return 0;}
VOID MtMusbCancel(struct mt_musb_runtime *r,ULONG c){(void)r;(void)c;cancels++;done=0;}
ULONG MtMusbTake(struct mt_musb_runtime *r,ULONG c,PVOID b,ULONG n,PULONG actual,PULONG status,PULONG toggle)
{(void)r;(void)c;(void)b;(void)n;if(!done)return 1;done=0;*actual=3;*status=0;*toggle=1;return 0;}
ULONG MtMusbRecoverToggle(struct mt_musb_runtime *r,ULONG c,PULONG t){(void)r;(void)c;*t=1;return 0;}
ULONG MtMusbRootQueue(struct mt_musb_runtime *r,ULONG kind){(void)r;root_kind=kind;return 0;}
ULONG MtMusbObserve(struct mt_musb_runtime *r,struct a04_hub_observation *o,PULONG c,PULONG kind,PULONG result)
{(void)r;*o=observation;*c=connected;*kind=root_kind;*result=root_result;root_kind=0;return fault;}
ULONG MtMusbFrame(struct mt_musb_runtime *r){(void)r;return 10;}
static VOID Complete(PVOID h,PVOID e,struct a04_mp_parameters *p,USBD_STATUS s,ULONG actual)
{(void)e;(void)s;(void)actual;completed++;completed_parameters=p;Packet.check(h); /* reentrant callback */}
int main(void)
{
 struct a04_hcd h;struct a04_mp_resources r={0};struct a04_mp_endpoint ep;
 struct a04_mp_properties e={0};struct a04_mp_parameters p={0};struct a04_mp_transfer t;
 struct a04_mp_sg sg={0};ULONG actual,status;UCHAR data[8];
 r.fdo=(PDEVICE_OBJECT)1;r.pdo=(PDEVICE_OBJECT)2;FillPacket();Packet.complete=Complete;Packet.async_callback=Schedule;Packet.invalidate_controller=InvalidateController;
 CHECK(Packet.start(&h,&r)==A04_MP_UNSUPPORTED&&!h.started&&!h.runtime&&opens==1);
 admission=STATUS_SUCCESS;CHECK(!Packet.start(&h,&r));CHECK(!Packet.port_power(&h,1));
 CHECK(!(h.port.status&A04_PORT_POWER)); /* accepting command is not proof */
 observation.qualified=observation.id_host=observation.overcurrent_clear=observation.vbus_verified=observation.endpoints_quiesced=1;
 connected=1;Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_CONNECTION);
 CHECK(!(h.port.status&A04_PORT_ENABLE));CHECK(!Packet.port_reset(&h,1));
 observation.reset_asserted_ms=50;Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_ENABLE);
 e.address=1;e.endpoint=0x81;e.type=3;e.packet=8;e.speed=1;e.period=1;
 CHECK(!Packet.open(&h,&e,&ep));Packet.set_state(&h,&ep,A04_EP_ACTIVE);p.bytes=8;sg.mapped_va=data;
 CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));done=1;Packet.poll_endpoint(&h,&ep);
 CHECK(completed==1&&completed_parameters==&p&&t.actual==3&&ep.toggle==1);
 CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));Packet.abort(&h,&ep,&t,&actual);
 CHECK(cancels==1&&!h.active&&actual==0&&completed==1&&ep.toggle_unknown);
 /* Caller can reuse every USBPORT object immediately after cancel/close. */
 RtlZeroMemory(&p,sizeof(p));RtlZeroMemory(&t,sizeof(t));Packet.poll_endpoint(&h,&ep);CHECK(completed==1);
 p.bytes=8;CHECK(!Packet.submit(&h,&ep,&p,&t,&sg));Packet.close(&h,&ep,FALSE);
 CHECK(cancels==2&&!h.active&&ep.state==A04_EP_CLOSED);
 {ULONG prior=async_count;ULONG wrong=epoch_copy+1;
 AsyncPoll(&h,&wrong);CHECK(async_count==prior&&!invalidations);
 AsyncPoll(&h,&epoch_copy);CHECK(async_count==prior+1&&invalidations==1);}
 fault=1;Packet.stop(&h,FALSE);CHECK(closes==1&&!h.runtime&&!h.started&&h.fault);
  {ULONG prior=async_count;AsyncPoll(&h,&epoch_copy);CHECK(async_count==prior);}
 return 0;
}

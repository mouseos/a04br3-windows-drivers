/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
/* Compiled with real WDK81 types on x64 and ARM32. Executed only on x64. */
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static ULONG completed,invalidated,physical_calls;static USBD_STATUS completion_status;
static struct a04_mp_parameters *last_parameters;
static VOID complete(PVOID h,PVOID e,struct a04_mp_parameters *t,USBD_STATUS s,ULONG n){(void)h;(void)e;(void)t;(void)n;completed++;completion_status=s;last_parameters=t;}
static VOID invalidate(PVOID h){(void)h;invalidated++;}
static UCHAR registers[0x500],fifo[64];static unsigned readpos;
static UCHAR read8(void *p,unsigned n){(void)p;return (n==0x24||n==0x20)?fifo[readpos++]:registers[n];}
static USHORT read16(void *p,unsigned n){(void)p;return registers[n]|((USHORT)registers[n+1]<<8);}
static void write8(void *p,unsigned n,UCHAR v){(void)p;registers[n]=v;}
static void write16(void *p,unsigned n,USHORT v){(void)p;if(n==2||n==4){registers[n]&=(UCHAR)~v;registers[n+1]&=(UCHAR)~(v>>8);return;}if(n==0x16&&(v&16))v=0;if(n==0x12&&(v&0x100))v=0;registers[n]=(UCHAR)v;registers[n+1]=(UCHAR)(v>>8);}
static ULONG start(PVOID p,struct a04_mp_resources *r,MT8167_USB20_IO *io){(void)p;(void)r;io->context=0;io->read8=read8;io->read16=read16;io->write8=write8;io->write16=write16;return 0;}
static ULONG stop(PVOID p){(void)p;return 0;}
static ULONG power(PVOID p,int on,struct a04_hub_observation *o){(void)p;(void)on;physical_calls++;RtlZeroMemory(o,sizeof(*o));return 1;}
static ULONG reset(PVOID p,struct a04_hub_observation *o){(void)p;(void)o;return 1;}
static ULONG control(PVOID p,struct a04_mp_endpoint *e,struct a04_mp_parameters *v,struct a04_mp_transfer *t){(void)p;(void)e;(void)v;(void)t;return 0;}
static ULONG abort_tx(PVOID p,struct a04_mp_endpoint *e,struct a04_mp_transfer *t,PULONG n){(void)p;(void)e;(void)t;*n=0;return 0;}
static VOID poll(PVOID p){(void)p;}
static ULONG frame(PVOID p){(void)p;return 5;}

int main(void)
{
 struct a04_hcd h;struct a04_mp_resources r;struct a04_mp_root root;
 struct a04_mp_endpoint ep;struct a04_mp_properties v;struct a04_mp_parameters tx;
 struct a04_mp_transfer transfer;struct a04_hcd_backend backend;struct a04_hub_observation observation;struct a04_mp_sg sg;UCHAR data[8];ULONG status;USHORT s;
 RtlZeroMemory(&r,sizeof(r));r.fdo=(PDEVICE_OBJECT)1;r.pdo=(PDEVICE_OBJECT)2;
 FillPacket();Packet.complete=complete;Packet.invalidate_root=invalidate;
 CHECK(Packet.version==3&&(Packet.flags&0x2010)==0x2010);
 CHECK(Packet.extension_bytes==sizeof(h));CHECK(Packet.start(&h,&r)==0&&physical_calls==0);
 Packet.root_data(&h,&root);CHECK(root.ports==1&&root.characteristics==9&&root.power_good==50);
 CHECK(Packet.root_status(&h,&s)==0&&s==1);CHECK(Packet.port_status(&h,1,&status)==0&&status==0);
 CHECK(Packet.port_status(&h,2,&status)!=0);
 CHECK(Packet.port_power(&h,1)==A04_MP_UNSUPPORTED&&h.port.status==0&&physical_calls==0);
 CHECK(Packet.port_reset(&h,1)!=0&&!(h.port.status&A04_PORT_ENABLE));
 CHECK(Packet.hub_status(&h,&status)==0&&status==0);Packet.root_enable(&h);CHECK(h.root_irq);
 RtlZeroMemory(&v,sizeof(v));v.address=1;v.endpoint=0x82;v.packet=8;v.max_packet=8;v.type=3;v.period=1;v.speed=1;
 CHECK(!Packet.open(&h,&v,&ep)&&Packet.get_state(&h,&ep)==A04_EP_PAUSED);
 Packet.set_state(&h,&ep,A04_EP_ACTIVE);CHECK(Packet.get_state(&h,&ep)==A04_EP_ACTIVE);
 Packet.toggle(&h,&ep,1);CHECK(ep.toggle==1);Packet.set_status(&h,&ep,1);CHECK(Packet.get_status(&h,&ep)==1);
 RtlZeroMemory(&tx,sizeof(tx));tx.bytes=8;
 CHECK(Packet.submit(&h,&ep,&tx,&transfer,0)!=0&&completed==0); /* no physical backend, no buffer access */
 CHECK(Packet.submit_iso(&h,&ep,&tx,&transfer,0)==A04_MP_UNSUPPORTED);
 v.hub=1;CHECK(Packet.open(&h,&v,&ep)==A04_MP_UNSUPPORTED);v.hub=0xffff;v.port=1;
 v.speed=2;v.type=2;v.packet=512;CHECK(Packet.open(&h,&v,&ep)==A04_MP_UNSUPPORTED);
 RtlZeroMemory(&backend,sizeof(backend));backend.version=1;backend.bytes=sizeof(backend);backend.start=start;
 CHECK(A04HcdSetBackend(&h,&backend)!=0&&physical_calls==0); /* incomplete owner never invoked */
 CHECK(A04HcdControlComplete(&h,&ep,&transfer,0,0)!=0&&completed==0);
 Packet.close(&h,&ep,FALSE);CHECK(ep.state==A04_EP_CLOSED);
 Packet.stop(&h,FALSE);CHECK(!h.started&&!h.port.status&&!physical_calls);
 CHECK(Packet.resume(&h)==A04_MP_UNSUPPORTED);
 CHECK(completion_status==0&&invalidated==0);
 /* Qualified fake owner drives the production Submit/Poll/Complete callbacks.
  * These are synthetic observations, not tablet evidence. */
 CHECK(!Packet.start(&h,&r));RtlZeroMemory(&backend,sizeof(backend));backend.version=1;backend.bytes=sizeof(backend);
 backend.start=start;backend.stop=stop;backend.power=power;backend.reset=reset;backend.control=control;
 backend.abort=abort_tx;backend.poll=poll;backend.frame=frame;
 CHECK(!A04HcdSetBackend(&h,&backend));CHECK(Packet.port_power(&h,1)==A04_MP_HARDWARE&&physical_calls==1&&h.port.status==0);
 RtlZeroMemory(&observation,sizeof(observation));CHECK(A04HcdPortObservation(&h,1,&observation));
 /* Test-only fixture creates a proven running port; no production setter exists. */
 CHECK(!a04_port_power(&h.port,1,1));CHECK(!a04_port_connect(&h.port,1));CHECK(!a04_port_reset_begin(&h.port));CHECK(!a04_port_reset_done(&h.port,0));
 v.address=1;v.endpoint=0x82;v.packet=8;v.max_packet=8;v.type=3;v.period=1;v.speed=1;
 CHECK(!Packet.open(&h,&v,&ep));Packet.set_state(&h,&ep,A04_EP_ACTIVE);
 RtlZeroMemory(&sg,sizeof(sg));sg.mapped_va=data;sg.count=1;
 CHECK(!Packet.submit(&h,&ep,&tx,&transfer,&sg));fifo[0]=0x31;fifo[1]=0x32;fifo[2]=0x33;
 registers[0x16]=1;registers[0x18]=3;registers[4]=2;Packet.poll_controller(&h);
 CHECK(completed==1&&completion_status==USBD_STATUS_SUCCESS&&last_parameters==&tx&&data[2]==0x33&&transfer.actual==3);
 CHECK(!Packet.submit(&h,&ep,&tx,&transfer,&sg));
 Packet.set_state(&h,&ep,A04_EP_PAUSED);CHECK(ep.state==A04_EP_PAUSED&&!h.fault);
 Packet.abort(&h,&ep,&transfer,&status);
 CHECK(!h.active&&completed==1&&status==0); /* abort doesn't double-complete */
 v.type=1;v.endpoint=0;CHECK(!Packet.open(&h,&v,&ep));Packet.set_state(&h,&ep,A04_EP_ACTIVE);
 tx.setup[0]=0x80;tx.setup[1]=6;tx.setup[6]=8;
 CHECK(!Packet.submit(&h,&ep,&tx,&transfer,&sg));
 CHECK(A04HcdControlComplete(&h,&ep,&transfer,USBD_STATUS_SUCCESS,8)); /* cannot forge stage completion */
 registers[0x12]=0;registers[0x13]=0;registers[2]=1;Packet.poll_controller(&h);
 CHECK(h.ep0.transfer.stage==EP0_IN&&completed==1);
 readpos=0;fifo[0]=1;fifo[1]=2;fifo[2]=3;registers[0x12]=1;registers[0x18]=3;registers[2]=1;
 Packet.poll_controller(&h);CHECK(h.ep0.transfer.stage==EP0_STATUS&&completed==1);
 registers[0x12]=0;registers[2]=1;Packet.poll_controller(&h);
 CHECK(completed==2&&last_parameters==&tx&&transfer.actual==3&&data[2]==3);
 CHECK(A04HcdControlComplete(&h,&ep,&transfer,0,8));
 Packet.stop(&h,FALSE);
 return 0;
}

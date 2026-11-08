/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
/* Compiled with real WDK81 types on x64 and ARM32. Executed only on x64. */
void *memset(void *p,int v,SIZE_T n){volatile UCHAR*d=p;SIZE_T i;for(i=0;i<n;i++)d[i]=(UCHAR)v;return p;}
void *memcpy(void *p,const void*s,SIZE_T n){volatile UCHAR*d=p;const volatile UCHAR*a=s;SIZE_T i;for(i=0;i<n;i++)d[i]=a[i];return p;}
#define CHECK(x) do{if(!(x))return __LINE__;}while(0)
static ULONG completed,invalidated,physical_calls;static USBD_STATUS completion_status;
static VOID complete(PVOID h,PVOID e,PVOID t,USBD_STATUS s,ULONG n){(void)h;(void)e;(void)t;(void)n;completed++;completion_status=s;}
static VOID invalidate(PVOID h){(void)h;invalidated++;}
static ULONG start(PVOID p,struct a04_mp_resources *r,MT8167_USB20_IO *io){(void)p;(void)r;(void)io;physical_calls++;return 1;}
int main(void)
{
 struct a04_hcd h;struct a04_mp_resources r;struct a04_mp_root root;
 struct a04_mp_endpoint ep;struct a04_mp_properties v;struct a04_mp_parameters tx;
 struct a04_mp_transfer transfer;struct a04_hcd_backend backend;ULONG status;USHORT s;
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
 RtlZeroMemory(&v,sizeof(v));v.address=1;v.endpoint=2;v.packet=8;v.max_packet=8;v.type=3;v.period=1;v.speed=1;
 CHECK(!Packet.open(&h,&v,&ep)&&Packet.get_state(&h,&ep)==A04_EP_PAUSED);
 Packet.set_state(&h,&ep,A04_EP_ACTIVE);CHECK(Packet.get_state(&h,&ep)==A04_EP_ACTIVE);
 Packet.toggle(&h,&ep,1);CHECK(ep.toggle==1);Packet.set_status(&h,&ep,1);CHECK(Packet.get_status(&h,&ep)==1);
 RtlZeroMemory(&tx,sizeof(tx));tx.bytes=8;
 CHECK(Packet.submit(&h,&ep,&tx,&transfer,0)!=0&&completed==0); /* no physical backend, no buffer access */
 CHECK(Packet.submit_iso(&h,&ep,&tx,&transfer,0)==A04_MP_UNSUPPORTED);
 v.hub=1;CHECK(Packet.open(&h,&v,&ep)==A04_MP_UNSUPPORTED);v.hub=0;
 v.speed=2;v.type=2;v.packet=512;CHECK(Packet.open(&h,&v,&ep)==A04_MP_UNSUPPORTED);
 RtlZeroMemory(&backend,sizeof(backend));backend.version=1;backend.bytes=sizeof(backend);backend.start=start;
 CHECK(A04HcdSetBackend(&h,&backend)!=0&&physical_calls==0); /* incomplete owner never invoked */
 CHECK(A04HcdControlComplete(&h,&ep,&transfer,0,0)!=0&&completed==0);
 Packet.close(&h,&ep,FALSE);CHECK(ep.state==A04_EP_CLOSED);
 Packet.stop(&h,FALSE);CHECK(!h.started&&!h.port.status&&!physical_calls);
 CHECK(Packet.resume(&h)==A04_MP_UNSUPPORTED);
 CHECK(completion_status==0&&invalidated==0);
 return 0;
}

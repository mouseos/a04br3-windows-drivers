/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define main LegacyHcdContract
#include "usbport_hcd_test.c"
#undef main
int main(void){
 static struct a04_hcd h;struct a04_mp_resources resources={0};static struct a04_mp_endpoint ep[17];
 struct a04_mp_properties e={0};static struct a04_mp_parameters p[17];static struct a04_mp_transfer t[17];struct a04_mp_sg sg={0};
 UCHAR buffer[17][8];ULONG status,actual,i,base;
 CHECK(!LegacyHcdContract());fault=0;complete_cookie=0;admission=STATUS_SUCCESS;root_kind=root_result=0;connected=0;
 resources.fdo=(PDEVICE_OBJECT)1;resources.pdo=(PDEVICE_OBJECT)2;CHECK(!Packet.start(&h,&resources));
 CHECK(!Packet.port_power(&h,1));connected=1;Packet.port_status(&h,1,&status);
 CHECK(!Packet.port_reset(&h,1));Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_ENABLE);
 e.endpoint=0x81;e.packet=8;e.type=3;e.speed=1;e.direction=0;e.period=1;e.hub=0xffff;
 RtlZeroMemory(p,sizeof(p));RtlZeroMemory(t,sizeof(t));
 for(i=0;i<8;i++){
  e.address=(USHORT)(i+1);if(i==7){e.type=1;e.endpoint=0;}
  CHECK(!Packet.open(&h,&e,&ep[i]));Packet.set_state(&h,&ep[i],A04_EP_ACTIVE);
 }
 e.type=3;e.endpoint=0x81;CHECK(Packet.open(&h,&e,&ep[8])==A04_MP_RESOURCES);
 for(i=0;i<17;i++){
  p[i].bytes=i%8==7?0:8;sg.mapped_va=buffer[i];
  if(i<16)CHECK(!Packet.submit(&h,&ep[i%8],&p[i],&t[i],&sg));
  else CHECK(Packet.submit(&h,&ep[0],&p[i],&t[i],&sg)==A04_MP_RESOURCES);
 }
 CHECK(AnyActive(&h));CHECK(Packet.submit(&h,&ep[0],&p[0],&t[16],&sg)==A04_MP_RESOURCES);
 /* Completing endpoint7 never completes or changes endpoints0..6. */
 base=completed;complete_cookie=ep[7].cookie;done=1;
 Packet.poll_endpoint(&h,&ep[0]);CHECK(completed==base);
 Packet.poll_endpoint(&h,&ep[7]);CHECK(completed==base+1&&completed_parameters==&p[7]&&ActiveSlot(&h,&ep[0])>=0);
 /* A queued/active abort detaches just that caller-owned context. */
 Packet.abort(&h,&ep[2],&t[2],&actual);CHECK(!actual&&ActiveSlot(&h,&ep[2])>=0&&ActiveSlot(&h,&ep[3])>=0);
 RtlZeroMemory(&t[2],sizeof(t[2]));RtlZeroMemory(&p[2],sizeof(p[2]));Packet.poll_endpoint(&h,&ep[2]);CHECK(completed==base+1);
 Packet.close(&h,&ep[3],FALSE);CHECK(ActiveSlot(&h,&ep[3])<0&&ActiveSlot(&h,&ep[4])>=0);
 complete_cookie=ep[4].cookie;done=1;Packet.poll_endpoint(&h,&ep[4]);CHECK(completed==base+2&&completed_parameters==&p[4]);
 Packet.stop(&h,FALSE);CHECK(!AnyActive(&h)&&!h.runtime);
 return 0;
}

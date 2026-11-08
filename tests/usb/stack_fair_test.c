/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define Query PhysicalQuery
#define main LegacyPhysicalStack
#include "musb_physical_test.c"
#undef main
#undef Query
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
static ULONG completions;static struct a04_mp_parameters *last;
static NTSTATUS ScheduleStack(PVOID a,ULONG b,PVOID c,ULONG d,A04_PORT_ASYNC_ROUTINE e)
{(void)a;(void)b;(void)c;(void)d;(void)e;return STATUS_SUCCESS;}
static VOID InvalidateStack(PVOID p,ULONG n){(void)p;(void)n;}
static VOID RootStack(PVOID p){(void)p;}
static VOID CompleteStack(PVOID h,PVOID ep,struct a04_mp_parameters *p,USBD_STATUS s,ULONG n)
{(void)ep;(void)s;(void)n;completions++;last=p;Packet.check(h);}
static VOID SweepStack(struct a04_hcd *h){test_now+=10000;TimerDpc(NULL,h->runtime,NULL,NULL);Work();Packet.poll_controller(h);}
int main(void){
 struct a04_hcd h;struct a04_mp_resources res={0};struct a04_mp_properties e={0};struct a04_mp_endpoint ep[4];
 struct a04_mp_transfer tr[6];struct a04_mp_parameters pa[6];struct a04_mp_sg sg={0};UCHAR data[6][32];ULONG status;unsigned i;
 Fixture();test_qualified=1;test_banked=1;RtlZeroMemory(test_bank,sizeof(test_bank));RtlZeroMemory(pa,sizeof(pa));
 FillPacket();Packet.async_callback=ScheduleStack;Packet.invalidate_controller=InvalidateStack;Packet.invalidate_root=RootStack;Packet.complete=CompleteStack;
 res.fdo=(PDEVICE_OBJECT)1;res.pdo=(PDEVICE_OBJECT)2;CHECK(!Packet.start(&h,&res));Work();
 CHECK(!Packet.port_power(&h,1));Work();Packet.port_status(&h,1,&status);test_mac[0xa]=0x10;SweepStack(&h);
 CHECK(!Packet.port_reset(&h,1));Work();Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_ENABLE);
 e.address=2;e.endpoint=0x81;e.type=3;e.direction=0;e.speed=2;e.period=8;e.packet=8;e.hub=0xffff;
 for(i=0;i<4;i++){e.address=(USHORT)(i+2);if(i==2){e.type=2;e.endpoint=2;e.direction=1;e.packet=512;}if(i==3){e.type=1;e.endpoint=0;e.packet=64;}
 CHECK(!Packet.open(&h,&e,&ep[i]));Packet.set_state(&h,&ep[i],A04_EP_ACTIVE);}
 for(i=0;i<5;i++){unsigned pipe=i<3?i:2;pa[i].bytes=31;sg.mapped_va=data[i];
 CHECK(!Packet.submit(&h,&ep[pipe],&pa[i],&tr[i],&sg));}
 Work();CHECK(h.runtime->lanes[1].hardware_job&&h.runtime->lanes[2].hardware_job&&h.runtime->lanes[3].hardware_job);
 /* Three FIFO-ordered BOT commands progress while two interrupt INs NAK. */
 for(i=2;i<5;i++){
  *(USHORT*)&test_bank[3][0x12]=0x100;*(USHORT*)(test_mac+2)|=8;SweepStack(&h);Packet.poll_endpoint(&h,&ep[2]);
  CHECK(last==&pa[i]&&tr[i].actual==31&&completions==i-1);Work();
 }
 CHECK(h.runtime->lanes[1].hardware_job&&h.runtime->lanes[2].hardware_job);
 /* Native hub EP0 is independent from both idle interrupt lanes. */
 pa[5].bytes=0;pa[5].setup[1]=9;CHECK(!Packet.submit(&h,&ep[3],&pa[5],&tr[5],NULL));Work();
 *(USHORT*)&test_bank[0][0x12]=0;*(USHORT*)(test_mac+2)|=1;SweepStack(&h);
 *(USHORT*)&test_bank[0][0x12]=0;*(USHORT*)(test_mac+2)|=1;SweepStack(&h);Packet.poll_endpoint(&h,&ep[3]);
 CHECK(last==&pa[5]&&completions==4);
 Packet.abort(&h,&ep[0],&tr[0],&status);RtlZeroMemory(&pa[0],sizeof(pa[0]));RtlZeroMemory(&tr[0],sizeof(tr[0]));SweepStack(&h);
 CHECK(h.runtime->lanes[1].job.state==MT_JOB_IDLE&&h.runtime->lanes[2].hardware_job);
 Packet.close(&h,&ep[1],FALSE);SweepStack(&h);Packet.stop(&h,FALSE);CHECK(!AnyActive(&h));test_banked=0;return 0;
}

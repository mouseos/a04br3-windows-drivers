/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Production HCD + PASSIVE backend, deterministic register/OS seam only. */
#define Query PhysicalQuery
#define main LegacyPhysicalStack
#include "musb_physical_test.c"
#undef main
#undef Query
#define A04_HCD_CONTRACT_TEST 1
#include "../../drivers/usb/mt8167-usb20/usbport_hcd.c"
static ULONG completions,last_status,last_actual;
static NTSTATUS Schedule(PVOID a,ULONG b,PVOID c,ULONG d,A04_PORT_ASYNC_ROUTINE e){(void)a;(void)b;(void)c;(void)d;(void)e;return STATUS_SUCCESS;}
static VOID Invalidate(PVOID a,ULONG b){(void)a;(void)b;}
static VOID Root(PVOID a){(void)a;}
static VOID Complete(PVOID h,PVOID ep,struct a04_mp_parameters *p,USBD_STATUS status,ULONG actual){(void)ep;(void)p;completions++;last_status=status;last_actual=actual;Packet.check(h);}
static VOID Sweep(struct a04_hcd *h){test_now+=10000;TimerDpc(NULL,h->runtime,NULL,NULL);Work();Packet.poll_controller(h);}
static VOID Event(struct a04_hcd*h,USHORT csr){*(USHORT*)&test_bank[0][0x12]=csr;*(USHORT*)(test_mac+2)|=1;Sweep(h);}
int main(void){static struct a04_hcd h;struct a04_mp_resources r={0};struct a04_mp_properties e={0};struct a04_mp_endpoint ep;struct a04_mp_parameters p={0};struct a04_mp_transfer tr;struct a04_mp_sg sg={0};UCHAR data[8]={0};ULONG status,base;ULONGLONG deadline;unsigned i;
 Fixture();test_qualified=1;test_banked=1;RtlZeroMemory(test_bank,sizeof(test_bank));FillPacket();Packet.async_callback=Schedule;Packet.invalidate_controller=Invalidate;Packet.invalidate_root=Root;Packet.complete=Complete;
 r.fdo=(PDEVICE_OBJECT)1;r.pdo=(PDEVICE_OBJECT)2;CHECK(!Packet.start(&h,&r));Work();CHECK(!Packet.port_power(&h,1));Work();Packet.port_status(&h,1,&status);test_mac[0xa]=0x10;Sweep(&h);CHECK(!Packet.port_reset(&h,1));Work();Packet.port_status(&h,1,&status);CHECK(h.port.status&A04_PORT_ENABLE);
 e.address=2;e.endpoint=0;e.type=1;e.speed=2;e.packet=64;e.hub=0xffff;CHECK(!Packet.open(&h,&e,&ep));Packet.set_state(&h,&ep,A04_EP_ACTIVE);
 p.bytes=8;p.setup[0]=0x80;p.setup[1]=6;p.setup[6]=8;sg.mapped_va=data;CHECK(!Packet.submit(&h,&ep,&p,&tr,&sg));Work();deadline=tr.admitted+50000000ull;Event(&h,0);CHECK(h.runtime->lanes[0].ep0.transfer.stage==EP0_IN);
 for(i=0;i<32;i++){ULONG out=test_fifo_out[0],in=test_fifo_in[0];Event(&h,0xa0);Packet.poll_endpoint(&h,&ep);CHECK(!completions&&h.runtime->lanes[0].job.deadline==deadline&&test_fifo_out[0]==out&&test_fifo_in[0]==in);}
 for(i=0;i<8;i++)test_fifo[0][i]=(UCHAR)(i+1);test_bank[0][0x18]=8;Event(&h,1);CHECK(h.runtime->lanes[0].ep0.transfer.stage==EP0_STATUS);
 Event(&h,0xc2);Packet.poll_endpoint(&h,&ep);CHECK(!completions);Event(&h,0);Packet.poll_endpoint(&h,&ep);CHECK(completions==1&&last_status==USBD_STATUS_SUCCESS&&last_actual==8&&data[7]==8);
 /* Device STALL reaches USBPORT as STALL, not generic XACT_ERROR. */
 RtlZeroMemory(&p,sizeof(p));p.setup[1]=9;p.setup[2]=1;CHECK(!Packet.submit(&h,&ep,&p,&tr,NULL));Work();Event(&h,4);Packet.poll_endpoint(&h,&ep);CHECK(completions==2&&last_status==USBD_STATUS_STALL_PID&&!ep.dispatch_blocked);
 /* A following class request remains eligible. NAK cannot reset its budget. */
 CHECK(!Packet.submit(&h,&ep,&p,&tr,NULL));Work();Event(&h,0);CHECK(h.runtime->lanes[0].ep0.transfer.stage==EP0_STATUS);deadline=tr.admitted+50000000ull;
 Event(&h,0xe0);Packet.poll_endpoint(&h,&ep);CHECK(completions==2&&h.runtime->lanes[0].job.deadline==deadline);
 test_now=deadline+1;Sweep(&h);Packet.poll_endpoint(&h,&ep);CHECK(completions==3&&last_status==USBD_STATUS_XACT_ERROR&&last_actual==0);
 /* Explicit Abort detaches before the worker observes a NAK/cancel. */
 CHECK(!Packet.submit(&h,&ep,&p,&tr,NULL));Work();Event(&h,0);Event(&h,0xe0);base=completions;Packet.abort(&h,&ep,&tr,&status);RtlZeroMemory(&tr,sizeof(tr));RtlZeroMemory(&p,sizeof(p));Sweep(&h);Packet.poll_endpoint(&h,&ep);CHECK(completions==base&&ActiveSlot(&h,&ep)<0);
 Packet.close(&h,&ep,FALSE);Sweep(&h);Packet.stop(&h,FALSE);test_banked=0;return 0;
}

/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define main LegacyPhysicalContract
#include "musb_physical_test.c"
#undef main
static VOID Pulse(struct mt_musb_runtime *r){test_now+=10000;TimerDpc(NULL,r,NULL,NULL);Work();}
static VOID AckOut(unsigned lane){*(USHORT*)&test_bank[lane][0x12]=0x100;*(USHORT*)(test_mac+2)|=(USHORT)(1u<<lane);}
static VOID AckIn(unsigned lane,unsigned count,UCHAR value){
 ULONG i;for(i=0;i<count;i++)test_fifo[lane][i]=value;test_fifo_in[lane]=0;
 *(USHORT*)&test_bank[lane][0x18]=(USHORT)count;*(USHORT*)&test_bank[lane][0x16]=0x201;
 *(USHORT*)(test_mac+4)|=(USHORT)(1u<<lane);
}
int main(void){
 struct mt_musb_runtime *r;struct a04_mp_properties e={0};struct a04_mp_parameters p={0};
 UCHAR data[1025];ULONG n,s,t,c,d,x;struct a04_hub_observation o;unsigned i;
 CHECK(!LegacyPhysicalContract());Fixture();test_qualified=1;test_banked=1;
 RtlZeroMemory(test_bank,sizeof(test_bank));RtlZeroMemory(test_fifo_out,sizeof(test_fifo_out));
 CHECK(NT_SUCCESS(MtMusbOpen((PDEVICE_OBJECT)1,&r)));Work();
 CHECK(!MtMusbRootQueue(r,A04_HUB_POWER_ON));Work();MtMusbObserve(r,&o,&c,&d,&x);
 test_mac[0xa]=0x10;Pulse(r);CHECK(r->connected&&!r->fault);
 e.address=2;e.endpoint=0x81;e.packet=8;e.speed=2;e.type=3;e.direction=0;e.period=8;e.hub=0xffff;p.bytes=8;
 CHECK(!MtMusbSubmit(r,100,&e,0,&p,data));Work();CHECK(r->lanes[1].hardware_job);
 /* Hub and keyboard can NAK indefinitely within each bounded transfer while
  * BOT OUT/IN and control still progress in the same sweep. */
 e.address=3;CHECK(!MtMusbSubmit(r,101,&e,0,&p,data));Work();
 e.address=4;e.type=2;e.endpoint=2;e.packet=512;e.direction=1;p.bytes=31;
 for(i=0;i<31;i++)data[i]=(UCHAR)i;
 CHECK(!MtMusbSubmit(r,102,&e,0,&p,data));data[0]=0xee;Work();
 CHECK(test_fifo_out[3]==31&&test_fifo[3][0]==0&&r->lanes[3].shadow[0]==0);
 CHECK(!r->fault&&r->lanes[1].hardware_job&&r->lanes[2].hardware_job);
 AckOut(3);Pulse(r);CHECK(!MtMusbTake(r,102,data,sizeof(data),&n,&s,&t)&&n==31&&!s);
 e.endpoint=0x82;e.direction=0;p.bytes=1024;
 CHECK(!MtMusbSubmit(r,103,&e,t,&p,data));Work();AckIn(3,512,0x5a);Pulse(r);
 CHECK(r->lanes[3].pio.actual==512&&r->lanes[3].job.state!=MT_JOB_DONE);
 AckIn(3,13,0x6b);Pulse(r);CHECK(!MtMusbTake(r,103,data,sizeof(data),&n,&s,&t)&&n==525&&!s&&data[511]==0x5a&&data[512]==0x6b);
 /* Control SET_CONFIGURATION alongside hub/keyboard interrupt requests. */
 e.endpoint=0;e.type=1;e.packet=64;e.direction=1;p.bytes=0;p.setup[0]=0;p.setup[1]=9;p.setup[2]=1;
 CHECK(!MtMusbSubmit(r,104,&e,0,&p,NULL));Work();CHECK(r->lanes[0].ep0.transfer.stage==EP0_SETUP);
 *(USHORT*)&test_bank[0][0x12]=0;*(USHORT*)(test_mac+2)|=1;Pulse(r);
 CHECK(r->lanes[0].ep0.transfer.stage==EP0_STATUS);*(USHORT*)&test_bank[0][0x12]=0;*(USHORT*)(test_mac+2)|=1;Pulse(r);
 CHECK(!MtMusbTake(r,104,NULL,0,&n,&s,&t)&&!n&&!s);
 /* Idle interrupt requests are normal pending URBs, not five-second
  * transfer failures. Fresh single-worker sweeps keep their watchdog live. */
 for(i=0;i<6000;i++)Pulse(r);
 CHECK(r->lanes[1].job.state==MT_JOB_RUNNING&&r->lanes[2].job.state==MT_JOB_RUNNING&&!r->fault);
 /* Every lane owns nonoverlapping TX/RX512; exhaustion is side-effect free. */
 e.type=3;e.endpoint=0x81;e.packet=8;e.direction=0;p.bytes=8;
 for(i=3;i<8;i++){e.address=(USHORT)(i+10);CHECK(!MtMusbSubmit(r,110+i,&e,0,&p,data));}
 Work();{ULONG writes=test_writes;CHECK(MtMusbSubmit(r,200,&e,0,&p,data)&&test_writes==writes);}
 for(i=1;i<8;i++){
  CHECK(*(USHORT*)&test_bank[i][0x64]==MT_MUSB_FIFO_BASE(i)/8);
  CHECK(*(USHORT*)&test_bank[i][0x66]==(MT_MUSB_FIFO_BASE(i)+512)/8);
  CHECK(test_bank[i][0x62]==6&&test_bank[i][0x63]==6);
 }
 /* Cancel one resident lane, then immediately discard all caller input.
  * The other resident packets are not flushed or reset for this abort. */
 {USHORT before=*(USHORT*)&test_bank[2][0x16];MtMusbCancel(r,100);RtlZeroMemory(&e,sizeof(e));RtlZeroMemory(&p,sizeof(p));Pulse(r);
 CHECK(r->lanes[1].job.state==MT_JOB_IDLE&&!r->lanes[1].hardware_job&&r->lanes[2].hardware_job&&*(USHORT*)&test_bank[2][0x16]==before);}
 AckIn(2,3,0x71);Pulse(r);CHECK(!MtMusbTake(r,101,data,sizeof(data),&n,&s,&t)&&n==3&&!s&&data[2]==0x71);
 for(i=3;i<8;i++){AckIn(i,1,(UCHAR)i);}Pulse(r);
 for(i=3;i<8;i++)CHECK(!MtMusbTake(r,110+i,data,sizeof(data),&n,&s,&t)&&n==1&&!s&&data[0]==i);
 CHECK(!MtMusbRecoverToggle(r,100,&t));CHECK(MtMusbClose(r));test_banked=0;
 return 0;
}

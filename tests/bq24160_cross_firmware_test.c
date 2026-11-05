/* SPDX-License-Identifier: MIT
 * Host-only integration using actual board UEFI producer and Windows consumer.
 * Every counter, I2C transaction and admission assertion here is simulated. */
#include <assert.h>
#include <string.h>
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#include "admission.h"
#include "broker_lifecycle.h"
#include "bqhr_diagnostic.h"
#include "mt8167_i2c.h"
#define NS 1000000000ULL
struct fake { uint64_t ns,tick; unsigned reads,writes,gauge; int fail, gauge_nack, corrupt_pec; uint16_t regs[128],fifo[4],reply[3],status; unsigned fifo_count,reply_count,reply_index; };
static uint64_t clock_ns(void *p){return ((struct fake *)p)->ns;}
static uint64_t counter(void *p){return ((struct fake *)p)->tick;}
static int rd(void *p,uint8_t r,uint8_t *out){
 static const uint8_t data[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};
 struct fake *f=p;++f->reads;if(f->fail)return -1;*out=data[r];return 0;
}
static int wr(void *p,uint8_t value){struct fake *f=p;assert(value==0x80);++f->writes;return f->fail;}
static uint8_t crc(uint8_t c,uint8_t v){unsigned i;c^=v;for(i=0;i<8;++i)c=(uint8_t)((c<<1)^((c&0x80)?7:0));return c;}
static int gauge(void *p,uint8_t address,uint8_t reg,uint8_t out[3]){
 struct fake *f=p;uint16_t value=reg==0x15?1:reg==9?4000:500;uint8_t c=0;
 assert(address==0x0b);++f->gauge;if(f->gauge_nack)return -1;out[0]=(uint8_t)value;out[1]=(uint8_t)(value>>8);
 c=crc(c,0x16);c=crc(c,reg);c=crc(c,0x17);c=crc(c,out[0]);out[2]=(uint8_t)(crc(c,out[1]) ^ (f->corrupt_pec ? 1 : 0));return 0;
}
/* Emulate the MT8167 controller boundary while executing both production PIO
 * clients. The fake provides no evidence of actual electrical timing or PnP. */
static uint16_t mmio_read(void *p,unsigned offset) {
 struct fake *f=p;
 if(offset==0x24)return 0;
 if(offset==0x0c)return f->status;
 if(offset==0x30)return (uint16_t)(f->reply_count<<4);
 if(offset==0)return f->reply[f->reply_index++];
 return f->regs[offset/2];
}
static void mmio_write(void *p,unsigned offset,uint16_t value) {
 struct fake *f=p;f->regs[offset/2]=value;
 if(offset==0x38){f->fifo_count=0;f->reply_count=0;f->reply_index=0;}
 if(offset==0x00){assert(f->fifo_count<4);f->fifo[f->fifo_count++]=value;}
 if(offset==0x24 && (value&1)) {
  uint8_t bytes[3],reg=(uint8_t)f->fifo[0];
  f->status=1;
  if(f->regs[0x04/2]==0xd6) {
   if(f->fifo_count==1) { if(rd(f,reg,bytes)){f->status=2;return;} f->reply[0]=bytes[0];f->reply_count=1; }
   else {assert(f->fifo_count==2 && reg==0);if(wr(f,(uint8_t)f->fifo[1]))f->status=2;}
  } else {
   assert(f->regs[0x04/2]==0x16 && f->fifo_count==1);
   if(gauge(f,0x0b,reg,bytes)){f->status=2;return;}
   f->reply[0]=bytes[0];f->reply[1]=bytes[1];f->reply[2]=bytes[2];f->reply_count=3;
  }
 }
}
static void mmio_delay(void *p,unsigned us){((struct fake *)p)->ns+=(uint64_t)us*1000;}
static uint64_t adapter_clock(void *p) {struct a04_gauge_mt8167 *a=p;return a->monotonic_ns(a->io->context);}
static void exercise_shared_faults(void) {
 struct fake f={0};struct a04_i2c_io io={&f,mmio_read,mmio_write,mmio_delay};
 struct a04_gauge_mt8167 adapter={.io=&io,.monotonic_ns=clock_ns};
 struct a04_bq_bus bus={&adapter,adapter_clock,a04_bq_mt8167_read,a04_bq_mt8167_kick};
 struct a04_gauge_bus gb={&adapter,a04_gauge_mt8167_read};
 struct a04_bq_owner owner={0};struct a04_bq_broker broker={0};
 struct a04_broker_endpoint endpoint={0};struct a04_broker_lifecycle life={0};
 struct a04_gauge_sample out={42,42,42};unsigned prior_reads,prior_writes;
 /* Simulated, not proven, prior owner admission. */
 owner.firmware_kicks=1;owner.handoff_pending=1;owner.handoff_deadline=20*NS;
 assert(!a04_broker_lifecycle_start(&life,&endpoint,&broker,&owner,&bus,&gb,0,40*NS,1));
 f.corrupt_pec=1;
 assert(a04_broker_lifecycle_sample_local(&life,5*NS,&out)==1);
 assert(out.voltage_mv==42 && !adapter.faulted && life.state==A04_LIFE_LIVE && f.writes==1);
 f.ns=10*NS;assert(!a04_broker_lifecycle_maintain(&life) && f.writes==2);
 f.corrupt_pec=0;assert(!a04_broker_lifecycle_sample_local(&life,15*NS,&out) && out.voltage_mv==4000);
 f.gauge_nack=1;assert(a04_broker_lifecycle_sample_local(&life,15*NS,&out)<0);
 assert(adapter.faulted && life.state==A04_LIFE_STOPPED && owner.stopped);
 prior_reads=f.reads;prior_writes=f.writes;f.gauge_nack=0;f.ns=20*NS;
 assert(a04_broker_lifecycle_maintain(&life));
 assert(f.reads==prior_reads && f.writes==prior_writes);
}
int main(void){
 struct fake f={0};struct a04_fw_owner firmware={0};struct a04_fw_io fw={&f,clock_ns,counter,rd,wr};
 struct a04_bq_owner windows={0},expired={0};
 struct a04_i2c_io io={&f,mmio_read,mmio_write,mmio_delay};
 struct a04_gauge_mt8167 adapter={.io=&io,.monotonic_ns=clock_ns};
 struct a04_bq_bus bus={&adapter,adapter_clock,a04_bq_mt8167_read,a04_bq_mt8167_kick};
 struct a04_gauge_bus gb={&adapter,a04_gauge_mt8167_read};struct a04_bq_broker broker={0};struct a04_broker_endpoint ep={0};
 struct a04_broker_lifecycle life={0};struct a04_bqhr_info info;struct a04_broker_request q={7,1,5*NS};
 struct a04_broker_response response;uint8_t record[128];
 exercise_shared_faults();
 f.tick=130000000;
 assert(!a04_fw_start(&firmware,&fw,13000000,20*NS,1,1,1,1));
 assert(a04_fw_tick(&firmware)==A04_BQ_OK&&f.reads==8&&f.writes==1);
 assert(!a04_fw_exit(&firmware,143000000,record,sizeof(record)));
 assert(a04_bqhr_classify(record,sizeof(record),&info)==A04_BQHR_OTHER_STATE);
 assert(a04_bq_admission_prepare(&expired,record,sizeof(record),NS,390000000,13000000,18*NS,7));
 assert(expired.stopped&&f.reads==8&&f.writes==1);
 assert(!a04_bq_admission_prepare(&windows,record,sizeof(record),NS,143000000,13000000,18*NS,7));
 assert(windows.handoff_pending&&windows.handoff_deadline==18*NS);
 f.ns=NS;
 assert(!a04_broker_lifecycle_start(&life,&ep,&broker,&windows,&bus,&gb,f.ns,18*NS,7));
 assert(!a04_broker_endpoint_dispatch(&ep,&q,&response));
 assert(response.result==A04_BROKER_OK&&response.sample.voltage_mv==4000);
 assert(f.reads==16&&f.writes==2&&f.gauge==3&&!windows.handoff_pending);
 assert(!a04_broker_lifecycle_quiesce(&life));
 q.sequence=2;assert(a04_broker_endpoint_dispatch(&ep,&q,&response)&&f.gauge==3);
 f.ns=11*NS;assert(!a04_broker_lifecycle_maintain(&life)&&f.writes==3);
 a04_broker_lifecycle_stop(&life);assert(ep.closed&&windows.stopped);
 assert(!adapter.faulted);
 {
  struct fake fault={0};struct a04_fw_owner bad_fw={0};struct a04_fw_io bad_bus={&fault,clock_ns,counter,rd,wr};
  struct a04_bq_owner rejected={0};fault.tick=130000000;fault.fail=1;
  assert(!a04_fw_start(&bad_fw,&bad_bus,13000000,20*NS,1,1,1,1));
  assert(a04_fw_tick(&bad_fw)==A04_BQ_READ_ERROR&&fault.reads==1&&!fault.writes);
  assert(a04_fw_exit(&bad_fw,143000000,record,sizeof(record)));
  assert(a04_bqhr_classify(record,sizeof(record),&info)==A04_BQHR_STOPPED);
  assert(a04_bq_admission_prepare(&rejected,record,sizeof(record),NS,143000000,13000000,18*NS,7));
  assert(rejected.stopped&&fault.reads==1&&!fault.writes);
 }
 return 0;
}

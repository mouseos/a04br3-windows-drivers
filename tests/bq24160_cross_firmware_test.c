/* SPDX-License-Identifier: MIT
 * Host-only integration using actual board UEFI producer and Windows consumer.
 * Every counter, I2C transaction and admission assertion here is simulated. */
#include <assert.h>
#include <string.h>
#include <Library/A04BR3BqFirmwareOwnerLib.h>
#include "admission.h"
#include "broker_lifecycle.h"
#include "bqhr_diagnostic.h"
#define NS 1000000000ULL
struct fake { uint64_t ns,tick; unsigned reads,writes,gauge; int fail; };
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
 assert(address==0x0b);++f->gauge;out[0]=(uint8_t)value;out[1]=(uint8_t)(value>>8);
 c=crc(c,0x16);c=crc(c,reg);c=crc(c,0x17);c=crc(c,out[0]);out[2]=crc(c,out[1]);return 0;
}
int main(void){
 struct fake f={0};struct a04_fw_owner firmware={0};struct a04_fw_io fw={&f,clock_ns,counter,rd,wr};
 struct a04_bq_owner windows={0},expired={0};struct a04_bq_bus bus={&f,clock_ns,rd,wr};
 struct a04_gauge_bus gb={&f,gauge};struct a04_bq_broker broker={0};struct a04_broker_endpoint ep={0};
 struct a04_broker_lifecycle life={0};struct a04_bqhr_info info;struct a04_broker_request q={7,1,5*NS};
 struct a04_broker_response response;uint8_t record[128];
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

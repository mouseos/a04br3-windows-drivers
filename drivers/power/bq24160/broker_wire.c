/* SPDX-License-Identifier: MIT */
#include "broker_wire.h"
#include <string.h>
static void put64(uint8_t *p,uint64_t v) { unsigned i;for(i=0;i<8;++i)p[i]=(uint8_t)(v>>(i*8)); }
static uint64_t get64(const uint8_t *p) { uint64_t v=0;unsigned i;for(i=0;i<8;++i)v|=(uint64_t)p[i]<<(i*8);return v; }
static int header(const uint8_t *p,unsigned size,unsigned kind)
{
    return p && size==A04_BROKER_WIRE_BYTES && p[0]=='A' && p[1]=='0' && p[2]=='4' && p[3]=='B' &&
           p[4]==1 && p[5]==kind && p[6]==A04_BROKER_WIRE_BYTES && !p[7];
}
static void init(uint8_t *p,unsigned kind)
{ memset(p,0,A04_BROKER_WIRE_BYTES);p[0]='A';p[1]='0';p[2]='4';p[3]='B';p[4]=1;p[5]=(uint8_t)kind;p[6]=A04_BROKER_WIRE_BYTES; }
int a04_broker_wire_request_encode(uint8_t *p,unsigned size,const struct a04_broker_request *r)
{
    if (!p || size!=A04_BROKER_WIRE_BYTES || !r || !r->epoch || !r->sequence || !r->deadline) return -1;
    init(p,1);put64(p+8,r->epoch);put64(p+16,r->sequence);put64(p+24,r->deadline);return 0;
}
int a04_broker_wire_request_decode(struct a04_broker_request *r,const uint8_t *p,unsigned size)
{
    struct a04_broker_request v;
    if (!r || !header(p,size,1) || get64(p+32) || get64(p+40)) return -1;
    v.epoch=get64(p+8);v.sequence=get64(p+16);v.deadline=get64(p+24);
    if (!v.epoch || !v.sequence || !v.deadline) return -1;
    *r=v;return 0;
}
int a04_broker_wire_response_encode(uint8_t *p,unsigned size,const struct a04_broker_response *r)
{
    if (!p || size!=A04_BROKER_WIRE_BYTES || !r || !r->epoch || !r->sequence ||
        (r->result!=A04_BROKER_OK && r->result!=A04_BROKER_REJECTED && r->result!=A04_BROKER_STOPPED) ||
        (r->result!=A04_BROKER_OK && (r->sample.voltage_mv || r->sample.indicator_tenths_percent || r->sample.percent))) return -1;
    init(p,2);put64(p+8,r->epoch);put64(p+16,r->sequence);p[24]=(uint8_t)r->result;
    p[25]=(uint8_t)r->sample.voltage_mv;p[26]=(uint8_t)(r->sample.voltage_mv>>8);
    p[27]=(uint8_t)r->sample.indicator_tenths_percent;p[28]=(uint8_t)(r->sample.indicator_tenths_percent>>8);p[29]=r->sample.percent;return 0;
}
int a04_broker_wire_response_decode(struct a04_broker_response *r,const uint8_t *p,unsigned size)
{
    struct a04_broker_response v; unsigned i;
    if (!r || !header(p,size,2)) return -1;
    for(i=30;i<A04_BROKER_WIRE_BYTES;++i)if(p[i])return -1;
    memset(&v,0,sizeof(v));v.epoch=get64(p+8);v.sequence=get64(p+16);
    if (!v.epoch || !v.sequence || p[24]>A04_BROKER_STOPPED) return -1;
    v.result=(enum a04_bq_broker_result)p[24];v.sample.voltage_mv=(uint16_t)(p[25]|((uint16_t)p[26]<<8));
    v.sample.indicator_tenths_percent=(uint16_t)(p[27]|((uint16_t)p[28]<<8));v.sample.percent=p[29];
    if(v.result!=A04_BROKER_OK && (v.sample.voltage_mv || v.sample.indicator_tenths_percent || v.sample.percent))return -1;
    *r=v;return 0;
}

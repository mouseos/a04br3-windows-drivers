/* SPDX-License-Identifier: MIT. Protocol execution tests, not real measurement. */
#include <assert.h>
#include <stdio.h>
#include "gauge.h"
struct bus {unsigned call,fail,corrupt;uint16_t power,method,temperature,after_power,after_method;};
static int read_pec(void*context,uint8_t address,uint8_t command,uint8_t out[3]){
 struct bus*b=context;unsigned bit,i;uint16_t value;uint8_t frame[5],crc=0;
 static const uint8_t commands[]={0x15,0x16,0x08,0x16,0x15};
 assert(address==0x0b&&b->call<5&&command==commands[b->call]);++b->call;
 if(b->call==b->fail)return -1;
 value=b->call==1?b->power:b->call==2?b->method:b->call==3?b->temperature:b->call==4?b->after_method:b->after_power;
 out[0]=(uint8_t)value;out[1]=(uint8_t)(value>>8);
 frame[0]=0x16;frame[1]=command;frame[2]=0x17;frame[3]=out[0];frame[4]=out[1];
 for(i=0;i<5;++i){crc^=frame[i];for(bit=0;bit<8;++bit)crc=(uint8_t)((crc<<1)^((crc&0x80)?7:0));}
 out[2]=(uint8_t)(crc^(b->call==b->corrupt));return 0;
}
static struct bus good(void){struct bus b={0,0,0,1,1,2982,1,1};return b;}
int main(void){struct bus b=good();struct a04_gauge_bus io={&b,read_pec};uint32_t value=99;unsigned i;
 assert(!a04_gauge_read_temperature(&io,&value)&&value==2982&&b.call==5);
 b=good();b.method=0;value=99;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_UNSUPPORTED&&value==99&&b.call==2);
 b=good();b.power=2;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_UNSUPPORTED&&b.call==1);
 b=good();b.after_method=0;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_UNSUPPORTED&&value==99);
 b=good();b.after_power=2;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_UNSUPPORTED&&value==99);
 for(i=1;i<=5;++i){b=good();b.fail=i;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_IO&&value==99);b=good();b.corrupt=i;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_IO&&value==99);}
 b=good();b.temperature=0x09e4;assert(!a04_gauge_read_temperature(&io,&value)&&value==0x09e4);
 b=good();b.temperature=0x0d04;assert(!a04_gauge_read_temperature(&io,&value)&&value==0x0d04);
 b=good();b.temperature=0xffff;value=99;assert(a04_gauge_read_temperature(&io,&value)==A04_GAUGE_INVALID&&value==99);
 puts("PASS: actual PEC temperature reader; thermistor/operational qualification, units, mode-change/read/PEC/range failures, unchanged failure output");return 0;
}

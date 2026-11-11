/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#define A04_VIDPN_UART_SNAPSHOT 1
#include "../tools/winpe/vidpn-uart-snapshot.h"
#include <assert.h>
#include <stdio.h>
static struct a04_vut_snapshot seed,first,second;
static struct a04_vut_binding binding;
static unsigned reads,packets,words,ended,records,mutate,fail_emit;
static uint32_t observed_summary[16];
static unsigned uart_bytes,uart_chunks;
static int send_mock(void *context,const char *line,unsigned bytes) {
 (void)context;assert(bytes<=94&&line[0]=='V'&&line[1]=='U'&&line[bytes-1]=='\n');
 uart_bytes+=bytes;uart_chunks++;return 0;
}
static void reset(void) {
 memset(&seed,0,sizeof(seed));memset(&binding,0,sizeof(binding));
 binding.run=51760;binding.pid=1428;binding.sys_verified=1;
 binding.source_sha1[0]=1;binding.sys_sha256[0]=2;
 reads=packets=words=ended=records=mutate=fail_emit=0;
 memset(observed_summary,0,sizeof(observed_summary));
}
static void value(unsigned i) {
 struct a04_vut_value *v=seed.value+i;uint32_t *w=v->w;
 v->present=1;v->type=3;v->bytes=432;
 w[0]=1;w[1]=432;w[2]=i?(i==43?1:1+(i-1)/6):0;
 w[3]=i&&i<43?(i-1)%6:0;w[4]=1;w[6]=7;w[7]=1;
 w[8]=42;w[10]=99;
 if(!i){w[20]=7;w[21]=4;}
 else if(i<43&&(i-1)%6==4)w[5]=0xc0000001u;
}
static unsigned read_mock(void *context,struct a04_vut_snapshot *out) {
 (void)context;memcpy(out,&seed,sizeof(seed));
 if(++reads==2&&mutate)out->value[6].w[30]++;
 return 0;
}
static int emit_mock(void *context,uint32_t event,const uint32_t *payload,uint32_t count) {
 (void)context;packets++;words+=count;
 if(fail_emit&&packets==fail_emit)return 1;
 assert(words<=A04_VUT_MAX_WORDS&&packets<=A04_VUT_MAX_PACKETS);
 if(event==A04_VUT_BIND){assert(count==32&&payload[3]==0&&payload[21]==7);}
 else if(event==A04_VUT_SUMMARY){assert(count==16);memcpy(observed_summary,payload,64);}
 else if(event==A04_VUT_RECORD){assert(count==36);records++;}
 else if(event==A04_VUT_END){assert(count==4&&payload[1]==records&&payload[2]==packets&&payload[3]==words);ended++;}
 else assert(event==A04_VUT_ERROR&&count==4);
 return 0;
}
static unsigned collect(void) {
 return a04_vut_collect(read_mock,NULL,&binding,emit_mock,NULL,&first,&second);
}
int main(void) {
 unsigned i;
 reset();assert(collect()==A04_VUT_MISSING&&!ended&&packets==1);
 reset();value(0);binding.pid=0;assert(collect()==A04_VUT_UNBOUND&&!ended);
 reset();value(0);seed.value[0].w[4]=0;seed.value[0].w[5]=0x103;
 assert(!collect()&&ended&&observed_summary[9]==0&&observed_summary[10]==0x103);
 reset();value(0);binding.sys_verified=0;memset(binding.source_sha1,0,20);memset(binding.sys_sha256,0,32);
 assert(!collect()&&ended); /* Unknown provenance is emitted as unknown, never promoted. */
 reset();value(0);value(6);mutate=1;assert(collect()==A04_VUT_CHANGED&&!ended);
 reset();value(0);value(6);seed.value[6].type=1;assert(collect()==A04_VUT_SHAPE&&!ended);
 reset();value(0);value(6);seed.value[6].bytes=428;assert(collect()==A04_VUT_SHAPE&&!ended);
 reset();value(0);value(43);seed.value[43].w[51]=1;assert(collect()==A04_VUT_SHAPE&&!ended);
 reset();value(0);value(5);seed.value[5].w[5]=0;assert(collect()==A04_VUT_SHAPE&&!ended);
 reset();value(0);value(6);seed.value[6].w[6]--;
 assert(!collect()&&ended&&observed_summary[3]==0&&observed_summary[4]==1&&records==0);
 assert(!observed_summary[12]&&!observed_summary[13]); /* Never absence/success proof. */
 reset();value(0);value(6);seed.value[6].w[4]=0;seed.value[6].w[5]=0x103;
 assert(!collect()&&ended&&observed_summary[5]==1&&records==1);
 reset();for(i=0;i<A04_VUT_VALUES;i++)value(i);
 assert(!collect()&&ended&&records==15&&packets==18&&words==592);
 assert(observed_summary[6]==7&&observed_summary[7]==1&&observed_summary[8]==127);
 {struct a04_vut_uart uart={send_mock,NULL,0,0,0};reads=0;
  assert(!a04_vut_collect(read_mock,NULL,&binding,a04_vut_uart_emit,&uart,&first,&second));
  assert(uart.words==592&&uart.packets==18&&uart_chunks==100&&uart_bytes<=9400);}
 reset();value(0);fail_emit=2;assert(collect()==A04_VUT_TRANSPORT&&!ended);
 puts("PASS: missing/unbound/epoch-mismatch/incomplete-start/torn/type/size/semantic/F0/stale/max44/transport");
 return 0;
}

/* Host-only: actual decode/debounce/report core; no hardware or WDF simulation. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/input/mt6392-keys/state.h"
static void feed(struct mt6392_keys_state *s,unsigned raw,unsigned count){while(count--)(void)mt6392_keys_sample(s,1,(uint16_t)raw);}
int main(void){struct mt6392_keys_state s;uint8_t report[2];
 mt6392_keys_init(&s);feed(&s,0xfff9,8);assert(!s.armed&&!s.value); /* held at boot */
 feed(&s,0xffff,2);assert(!s.armed);feed(&s,0xffff,1);assert(s.armed&&!s.value);
 feed(&s,0xfffd,2);assert(!s.value);feed(&s,0xffff,1);feed(&s,0xfffd,2);assert(!s.value);feed(&s,0xfffd,1);assert(s.value==(1u<<A04_POWER));
 mt6392_keys_report(&s,1,report);assert(report[0]==1&&report[1]==1);mt6392_keys_report(&s,2,report);assert(report[0]==2&&report[1]==0);
 feed(&s,0xfff9,3);assert(s.value==MT6392_KEYS_MASK);mt6392_keys_report(&s,2,report);assert(report[1]==1);
 assert(mt6392_keys_sample(&s,0,0));assert(s.failed&&!s.value);feed(&s,0xfff9,8);assert(!s.value); /* no guessed recovery */
 mt6392_keys_init(&s);feed(&s,0xffff,3);feed(&s,0xfffb,3);assert(s.value==(1u<<A04_VOLUME_DOWN));feed(&s,0xffff,3);assert(!s.value);
 /* Every non-key CHRSTATUS bit is ignored by the reused real register decoder. */
 for(unsigned raw=0;raw<65536;++raw){unsigned expected=((raw&2)?0:1u<<A04_POWER)|((raw&4)?0:1u<<A04_VOLUME_DOWN);assert((a04_buttons_decode(0xffff,(uint16_t)raw)&MT6392_KEYS_MASK)==expected);}
 puts("MT6392 actual decode/debounce/reports: held-boot, bounce, press/release, fault latch, D0 restart, all65536 raw mappings PASS");return 0;}

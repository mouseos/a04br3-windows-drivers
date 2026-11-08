/* Host execution of production decode/debounce; no simulated MMIO claim. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/input/mt8167-keypad/state.h"
static void feed(struct mt8167_keypad_state *s,int pv,unsigned p,int kv,unsigned k,unsigned n){while(n--)mt8167_keypad_sample(s,pv,(uint16_t)p,kv,(uint16_t)k);}
int main(void){struct mt8167_keypad_state s;uint8_t r[2];
 mt8167_keypad_init(&s);feed(&s,1,0xfff9,1,0xfffc,4);
 assert(!s.pmic.armed&&!s.keypad.armed&&!mt8167_keypad_value(&s));
 feed(&s,1,0xffff,1,0xffff,3);assert(s.pmic.armed&&s.keypad.armed);
 feed(&s,1,0xfff9,1,0xfffc,2);assert(!mt8167_keypad_value(&s));
 feed(&s,1,0xffff,1,0xffff,1);feed(&s,1,0xfff9,1,0xfffc,3);
 assert(mt8167_keypad_value(&s)==15);
 for(unsigned id=1;id<=4;++id){mt8167_keypad_report(&s,id,r);assert(r[0]==id&&r[1]==1);}
 feed(&s,0,0,1,0xfffc,1);assert(s.pmic.failed&&!s.pmic.value&&s.keypad.value);
 feed(&s,1,0xfff9,1,0xffff,3);assert(!mt8167_keypad_value(&s));
 feed(&s,1,0xfff9,1,0xfffe,3);mt8167_keypad_report(&s,3,r);assert(r[1]==1);
 feed(&s,1,0xfff9,0,0,1);assert(!mt8167_keypad_value(&s)&&s.keypad.failed);
 mt8167_keypad_init(&s);feed(&s,1,0xffff,0,0,3);assert(s.pmic.armed&&s.keypad.failed);
 feed(&s,1,0xfffd,1,0xfffc,3);assert(mt8167_keypad_value(&s)==(1u<<A04_POWER));
 puts("PASS held startup, debounce bounce, four reports, independent fault release/latch, D0 restart");
}

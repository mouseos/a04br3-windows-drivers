/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stddef.h>
#include "../drivers/gpu/mt8167/display-transport/panel_timing.h"
int main(void){
 struct a04_panel_timing t={A04_PANEL_TIMING_MAGIC,1,64,1,1,800,1280,960,1300,5990,100,{0}};
 struct a04_panel_timing bad;struct a04_panel_frequencies f;
 assert(sizeof(t)==64&&offsetof(struct a04_panel_timing,fps100)==36&&offsetof(struct a04_panel_timing,reserved)==44);
 assert(a04_panel_timing_frequencies(&t,&f));
 assert(f.vnum==599&&f.vden==10&&f.hnum==77870&&f.hden==1&&f.pixel_rate==74755200);
 t.fps100=6008;assert(a04_panel_timing_frequencies(&t,&f));
 assert(f.vnum==1502&&f.vden==25&&f.hnum==78104&&f.hden==1&&f.pixel_rate==74979840);
 /* Exact rational cross-products for the board-admitted rate interval.
  * Rates here exercise arithmetic only, not supported physical panel modes. */
 for(uint32_t rate=4500;rate<=7500;rate++){
  t.fps100=rate;assert(a04_panel_timing_frequencies(&t,&f));
  assert((uint64_t)f.vnum*100==(uint64_t)rate*f.vden);
  assert((uint64_t)f.hnum*100==(uint64_t)rate*1300*f.hden);
  assert(f.pixel_rate*100==(uint64_t)rate*1300*960);
 }
 t.fps100=5990;
 for(unsigned i=0;i<16;i++){
  bad=t;((uint32_t *)&bad)[i]^=1;
  /* fps100 is dynamic, not a pinned nominal rate. */
  if(i==9)assert(a04_panel_timing_frequencies(&bad,&f));
  else assert(!a04_panel_timing_frequencies(&bad,&f));
 }
 bad=t;bad.fps100=0;assert(!a04_panel_timing_frequencies(&bad,&f));
 bad=t;bad.fps100=4499;assert(!a04_panel_timing_frequencies(&bad,&f));
 bad=t;bad.fps100=7501;assert(!a04_panel_timing_frequencies(&bad,&f));
 bad=t;bad.fps100=0xffffffffu;assert(!a04_panel_timing_frequencies(&bad,&f));
 assert(!a04_panel_timing_frequencies(0,&f)&&!a04_panel_timing_frequencies(&t,0));
 return 0;
}

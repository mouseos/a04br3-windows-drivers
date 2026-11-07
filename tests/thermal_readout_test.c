/* SPDX-License-Identifier: GPL-2.0-only */
#include "readout.h"
#include <assert.h>
#include <string.h>
int main(void) {
 struct a04_thermal_readout_state s={0};
 struct a04_thermal_sample v={{32000,35000,33000},35000,100};
 struct a04_thermal_readout_v1 out,old;
 memset(&out,0xa5,sizeof out);old=out;
 assert(a04_readout_get(&s,100,1000,&out)<0 && !memcmp(&out,&old,sizeof out));
 assert(!a04_readout_publish(&s,&v));
 assert(!a04_readout_get(&s,110,1000,&out) && out.version==1 && out.sequence==1 && out.sensor_mc[1]==35000 && out.age_ms==10);
 old=out;assert(a04_readout_get(&s,99,1000,&out)<0 && !memcmp(&out,&old,sizeof out));
 assert(a04_readout_get(&s,1101,1000,&out)<0 && !memcmp(&out,&old,sizeof out));
 v.hottest_mc=30000;assert(a04_readout_publish(&s,&v)<0 && !s.valid);v.hottest_mc=35000;assert(!a04_readout_publish(&s,&v));
 v.time_ms=99;assert(a04_readout_publish(&s,&v)<0 && !s.valid);v.time_ms=100;assert(!a04_readout_publish(&s,&v));v.time_ms=101;
 v.mc[1]=150001;assert(a04_readout_publish(&s,&v)<0 && !s.valid);v.mc[1]=35000;
 assert(!a04_readout_publish(&s,&v) && s.sequence==4);
 a04_readout_invalidate(&s);assert(a04_readout_get(&s,101,1000,&out)<0);
 assert(!a04_readout_publish(&s,&v) && s.sequence==5);
 return 0;
}

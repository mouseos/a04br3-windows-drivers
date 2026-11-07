#include <assert.h>
#include <stdint.h>
#include "codec_dc.h"
#include "codec_calibration.h"
struct mock {uint32_t comp,ctrl;unsigned writes,delays,fail;int16_t last;};
static int rd(void *c,unsigned o,uint32_t *v){struct mock*m=c;*v=o==0xf0c?m->comp:m->ctrl;return 0;}
static int wr(void *c,unsigned o,uint32_t v){struct mock*m=c;if(++m->writes==m->fail)return -1;if(o==0xf0c){m->comp=v;m->last=(int16_t)v;}else {m->ctrl=(v&~2u)|((v&0x200u)?2u:0u);}return 0;}
static int wait(void *c,unsigned us){struct mock*m=c;assert(us==400 || us==2000);m->delays++;return 0;}
int main(void){struct mock m={0};struct a04_codec_dc s={0};struct a04_codec_dc_io io={&m,rd,wr,wait};int16_t code;int32_t samples[22],avg;unsigned i;
 for(i=0;i<22;i++)samples[i]=1000000;
 assert(!a04_codec_nominal_hp_uv(2048,&avg) && avg==750000);
 assert(!a04_codec_nominal_hp_uv(4095,&avg) && avg==1499633);
 assert(a04_codec_nominal_hp_uv(4096,&avg));
 samples[0]=0;samples[21]=1500000;
 assert(!a04_codec_average_hp_uv(samples,22,&avg) && avg==1000000);
 assert(a04_codec_average_hp_uv(samples,21,&avg));
 samples[1]=-1;assert(a04_codec_average_hp_uv(samples,22,&avg));
 samples[1]=1500001;assert(a04_codec_average_hp_uv(samples,22,&avg));
 assert(!a04_codec_calculate_comp(1000000,1001000,0,&code) && code==48);
 assert(!a04_codec_calculate_comp(1001000,1000000,0,&code) && code==-48);
 assert(!a04_codec_calculate_comp(1000000,1000000,7,&code) && code==0);
 assert(a04_codec_calculate_comp(0,1000,8,&code));
 assert(a04_codec_calculate_comp(INT32_MAX,INT32_MIN,0,&code));
 assert(!a04_codec_dc_ramp(&s,&io,-3000,3000,0));assert(m.last==3000 && m.delays==4 && !s.poisoned);
 assert(!a04_codec_dc_ramp(&s,&io,3000,-3000,3));assert(m.last==-3000 && m.delays==20);
 assert(a04_codec_dc_ramp(&s,&io,0,0,4));m.fail=m.writes+2;assert(a04_codec_dc_set(&s,&io,0) && s.poisoned);assert(a04_codec_dc_set(&s,&io,0));return 0;}

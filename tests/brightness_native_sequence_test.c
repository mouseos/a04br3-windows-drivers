#include <assert.h>
#include <string.h>
#include "../tools/display/brightness_sequence.h"
struct mock {unsigned original,value,sets[4],n,reads,pauses;int mode;};
static int snapshot(void *v,unsigned *original){struct mock *m=v;*original=m->original;return m->mode==1?-1:0;}
static int set(void *v,unsigned value){struct mock *m=v;assert(m->n<4);m->sets[m->n++]=value;m->value=value;
 return (m->mode==2 && value==30)||(m->mode==3 && value==70)||(m->mode==5 && value==50)?-1:0;}
static int read_level(void *v,unsigned *value){struct mock *m=v;m->reads++;*value=m->mode==4 && m->value==70?69:m->value;return m->mode==8 && m->value==70?-1:0;}
static int record(void *v,const char *phase,unsigned requested,unsigned observed,int result)
{struct mock *m=v;(void)requested;(void)observed;(void)result;return (m->mode==6 && !strcmp(phase,"original-percent")) || (m->mode==9 && !strcmp(phase,"set-percent"))?-1:0;}
static void pause_read(void *v){((struct mock *)v)->pauses++;}
int main(void){struct mock m;unsigned i,result;struct a04_brightness_ops o={&m,snapshot,set,read_level,record,pause_read};
 for(i=0;i<10;i++) {
  memset(&m,0,sizeof(m));m.original=50;m.value=50;m.mode=(int)i;if(i==7)m.original=0;
  result=a04_brightness_once(&o);
  if(i==0){assert(!result && m.n==3 && m.sets[0]==30 && m.sets[1]==70 && m.sets[2]==50 && m.reads==3);}
  else if(i==1 || i==6 || i==7){assert(result && m.n==0 && m.reads==0);}
  else {assert(result && m.sets[m.n-1]==50 && m.value==50);if(i==2)assert(m.n==2 && m.sets[0]==30);if(i==4)assert(m.reads==6 && m.pauses==3);if(i==5)assert(result&2);}
 }
 return 0;
}

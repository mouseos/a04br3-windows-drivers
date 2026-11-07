#include <assert.h>
#include <string.h>
#include "../tools/display/brightness_wmi_result.h"
struct mock {int mode,execs,releases,calls,decoded,secured;unsigned method_value;};
static int exec(void *v,void **direct,void **call){struct mock *m=v;m->execs++;if(m->mode==5)return (int)0x8000ffffu;if(m->mode==0 || m->mode==2)*direct=(void*)1;if(m->mode==1 || m->mode==2 || m->mode==4)*call=(void*)2;return 0;}
static int secure(void *v,void*c){struct mock*m=v;assert(c==(void*)2);m->secured++;return 0;}
static int result(void*v,void*c,void**o){struct mock*m=v;assert(c==(void*)2);m->calls++;if(m->mode==4)return (int)0x800705b4u;*o=(void*)3;return 0;}
static int value(void*v,void*o,unsigned*out){struct mock*m=v;assert(o==(void*)1 || o==(void*)3);m->decoded++;*out=m->method_value;return 0;}
static void release(void*v,void*o){struct mock*m=v;assert(o==(void*)1 || o==(void*)3);m->releases++;}
static void release_call(void*v,void*c){struct mock*m=v;assert(c==(void*)2);m->releases++;}
static void trace(void*v,const char*p,unsigned n,int hr){(void)v;(void)p;(void)n;(void)hr;}
int main(void){struct mock m;struct a04_wmi_result_ops o={&m,exec,secure,result,value,release,release_call,trace};unsigned ret;int hr;
 for(int mode=0;mode<6;mode++) {memset(&m,0,sizeof(m));m.mode=mode;ret=0;hr=a04_brightness_method_result(&o,&ret);assert(m.execs==1);
  if(mode<3){assert(!hr && ret==0 && m.decoded==1);assert(m.releases==(mode==0?1:2));if(mode==2)assert(!m.calls && !m.secured);}
  else {assert(hr && ret==~0u && !m.decoded);if(mode==5)assert((unsigned)hr==0x8000ffffu);}
 }
 memset(&m,0,sizeof(m));m.method_value=5;assert(!a04_brightness_method_result(&o,&ret) && ret==5);return 0;
}

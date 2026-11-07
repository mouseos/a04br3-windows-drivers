#include <assert.h>
#include <string.h>
#include "../tools/display/brightness_wmi_result.h"
struct mock {int mode,execs,releases,calls,decoded,secured,completed;unsigned method_value;};
static int exec(void *v,void **direct,void **call){struct mock*m=v;m->execs++;if(m->mode==5)return (int)0x8000ffffu;if(m->mode==0||m->mode==2)*direct=(void*)1;if(m->mode!=0&&m->mode!=3)*call=(void*)2;return 0;}
static int secure(void*v,void*c){struct mock*m=v;assert(c==(void*)2);m->secured++;return 0;}
static int completion(void*v,void*c,int*out){struct mock*m=v;assert(c==(void*)2);m->completed++;if(m->mode==6)return (int)0x800705b4u;*out=m->mode==7?(int)0x80041008u:0;return 0;}
static int result(void*v,void*c,void**o){struct mock*m=v;assert(c==(void*)2&&m->completed==1);m->calls++;if(m->mode==4)return (int)0x800705b4u;if(m->mode!=8)*o=(void*)3;return 0;}
static int value(void*v,void*o,unsigned*out){struct mock*m=v;assert(o==(void*)1||o==(void*)3);m->decoded++;*out=m->method_value;return m->mode==9?(int)0x80041005u:0;}
static void release(void*v,void*o){struct mock*m=v;assert(o==(void*)1||o==(void*)3);m->releases++;}
static void release_call(void*v,void*c){struct mock*m=v;assert(c==(void*)2);m->releases++;}
static void trace(void*v,const char*p,unsigned n,int hr){(void)v;(void)p;(void)n;(void)hr;}
int main(void){struct mock m;struct a04_wmi_result_ops o={&m,exec,secure,completion,result,value,release,release_call,trace};unsigned ret;int hr;
 for(int mode=0;mode<10;mode++){
  memset(&m,0,sizeof(m));m.mode=mode;hr=a04_brightness_method_result(&o,&ret);assert(m.execs==1);
  if(mode<3){assert(!hr&&ret==0&&m.decoded==1);assert(m.releases==(mode==0?1:2));if(mode==2)assert(!m.calls&&m.completed==1);}
  else {assert(hr);if(mode==5)assert((unsigned)hr==0x8000ffffu);if(mode==7)assert((unsigned)hr==0x80041008u&&!m.calls&&!m.decoded);if(mode==6)assert(!m.calls&&!m.decoded);if(mode==8)assert((unsigned)hr==0x8004100fu&&!m.decoded);}
 }
 memset(&m,0,sizeof(m));m.method_value=5;assert(!a04_brightness_method_result(&o,&ret)&&ret==5);
 return 0;
}

#include <assert.h>
#include "board_host.h"
typedef struct {int denied,writes,reads,releases,fail_write,fail_read,fail_release,never_ready;unsigned delays;} Mock;
static int acquire(void *p){return ((Mock*)p)->denied;}
static int release(void *p){Mock*m=p;m->releases++;return m->fail_release;}
static int read16(void *p,unsigned off,unsigned short *v){Mock*m=p;assert(off==0x0704);m->reads++;if(m->fail_read)return -1;*v=m->never_ready?0:(unsigned short)(0x8000|1200);return 0;}
static int write16(void *p,unsigned off,unsigned short v){Mock*m=p;assert(off==0x0734 && v==4);m->writes++;return m->fail_write;}
static void delay(void *p,unsigned us){Mock*m=p;assert(us==100);m->delays++;}
int main(void){Mock m={0};A04_MT6392_VCDT d={&m,acquire,release,read16,write16,delay,0,0};A04_MT6392_VCDT_SAMPLE s={1,2,3};
 m.denied=1;assert(A04Mt6392VcdtRead(&d,&s)==-1 && !m.writes && s.raw==1);m.denied=0;
 assert(!A04Mt6392VcdtRead(&d,&s) && s.raw==1200 && s.adc_mv==527 && s.charger_mv==4986 && m.reads==2 && m.releases==1 && !d.held);
 m.fail_write=1;assert(A04Mt6392VcdtRead(&d,&s)==-2 && d.held && d.uncertain && m.releases==1);assert(A04Mt6392VcdtRead(&d,&s)==-1);
 m.fail_write=0;d.held=d.uncertain=0;m.never_ready=1;m.reads=0;assert(A04Mt6392VcdtRead(&d,&s)==-2 && d.held && m.reads==100 && m.releases==1);
 d.held=d.uncertain=0;m.never_ready=0;m.fail_release=1;s.raw=7;s.adc_mv=8;s.charger_mv=9;
 assert(A04Mt6392VcdtRead(&d,&s)==-2 && d.held && d.uncertain && s.raw==7 && s.adc_mv==8 && s.charger_mv==9);
 return 0;}

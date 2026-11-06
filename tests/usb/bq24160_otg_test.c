#include <assert.h>
#include "board_host.h"
typedef struct { unsigned char reg; int owner_bad,switch_bad,write_fail,read_fail,read_count,writes,drop_lock; } Mock;
static int owner(void *p){return ((Mock*)p)->owner_bad;}
static int off(void *p){return ((Mock*)p)->switch_bad;}
static int rd(void *p,unsigned char *v){Mock*m=p;m->read_count++;if(m->read_fail)return -1;*v=m->reg;return 0;}
static int wr(void *p,unsigned char v){Mock*m=p;m->writes++;assert(!(v&~9u));if(!m->write_fail)m->reg=(unsigned char)((m->reg&~9u)|(v&9u));if(m->drop_lock)m->reg&=(unsigned char)~8u;return m->write_fail?-1:0;}
int main(void){Mock m={0};A04_BQ24160_OTG_LOCK b={&m,owner,rd,wr,off,0,0,0};
 m.owner_bad=1;assert(A04Bq24160OtgAcquire(&b)==-1 && !m.writes && !m.read_count);m.owner_bad=0;
 m.switch_bad=1;assert(A04Bq24160OtgAcquire(&b)==-1 && !m.writes);m.switch_bad=0;
 m.reg=0xf9;assert(A04Bq24160OtgAcquire(&b)==-1 && !m.writes);m.reg=0xf1;
 assert(!A04Bq24160OtgAcquire(&b) && b.held && !b.uncertain && (m.reg&9)==9);
 m.switch_bad=1;assert(A04Bq24160OtgRelease(&b) && b.held && b.uncertain && (m.reg&8));m.switch_bad=0;
 assert(!A04Bq24160OtgRelease(&b) && !b.held && !b.uncertain && (m.reg&9)==1);
 m.write_fail=1;assert(A04Bq24160OtgAcquire(&b)==-2 && b.held && b.uncertain);m.write_fail=0;
 assert(!A04Bq24160OtgRelease(&b) && !b.held);
 m.drop_lock=1;assert(A04Bq24160OtgAcquire(&b)==-2 && b.held && b.uncertain);
 m.drop_lock=0;assert(!A04Bq24160OtgRelease(&b) && !b.held);
 m.read_fail=1;assert(A04Bq24160OtgAcquire(&b)==-1 && !b.held);m.read_fail=0;
 assert(!A04Bq24160OtgAcquire(&b));m.owner_bad=1;
 assert(A04Bq24160OtgRelease(&b) && b.held && b.uncertain && (m.reg&8));m.owner_bad=0;
 assert(!A04Bq24160OtgRelease(&b) && !b.held);
 assert(!A04Bq24160OtgAcquire(&b));m.reg&=(unsigned char)~8u;
 assert(A04Bq24160OtgRelease(&b) && b.held && b.uncertain);return 0; }

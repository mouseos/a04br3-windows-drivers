#include <assert.h>
#include <string.h>
#include "keyboard_rx.h"
typedef struct { unsigned char b[0x500],fifo[8]; unsigned pos; unsigned short csr; } Mock;
static unsigned char r8(void *v,unsigned a) { Mock*m=v; return a==0x24 ? m->fifo[m->pos++] : m->b[a]; }
static unsigned short r16(void *v,unsigned a) { Mock*m=v; return a==0x16 ? m->csr : (unsigned short)(m->b[a]|m->b[a+1]<<8); }
static void w8(void *v,unsigned a,unsigned char n) { ((Mock*)v)->b[a]=n; }
static void w16(void *v,unsigned a,unsigned short n) { Mock*m=v; if(a==0x16) m->csr=n; else { m->b[a]=(unsigned char)n;m->b[a+1]=(unsigned char)(n>>8); } }
int main(void) {
 Mock m={0}; MT8167_USB20_IO io={&m,r8,r16,w8,w16}; unsigned char report[8];
 assert(KbRxConfigure(&io,1,2,8,10,0x80)==0); assert(m.b[0x0e]==1 && m.b[0x1c]==0xb2 && m.b[0x48c]==1);
 assert(KbRxConfigure(&io,1,2,16,10,0xc0)==-1);
 m.csr=0; assert(KbRxRequest(&io)==0); assert(m.csr&0x20);
 m.csr=1; m.b[0x18]=8; memset(m.fifo,0x2a,8); assert(KbRxComplete(&io,report)==1 && report[7]==0x2a);
 m.csr=8; assert(KbRxComplete(&io,report)==2);
 m.csr=0x40; assert(KbRxComplete(&io,report)==-1);
 m.csr=1; m.b[0x18]=9; assert(KbRxComplete(&io,report)==-1);
 return 0;
}

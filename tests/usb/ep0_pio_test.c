#include <assert.h>
#include <string.h>
#include "ep0_pio.h"
typedef struct { unsigned short csr; unsigned char in[64],out[128]; unsigned inpos,outpos,count,index; } Mock;
static unsigned char rd8(void *p,unsigned r) { Mock *m=p; if(r==0x18)return (unsigned char)m->count; assert(r==0x20); return m->in[m->inpos++]; }
static unsigned short rd16(void *p,unsigned r) { Mock *m=p; assert(r==0x12); return m->csr; }
static void wr8(void *p,unsigned r,unsigned char v) { Mock *m=p; if(r==0x0e)m->index=v; else { assert(r==0x20); m->out[m->outpos++]=v; } }
static void wr16(void *p,unsigned r,unsigned short v) { Mock *m=p; assert(r==0x12); m->csr=v; }
int main(void) {
    Mock m={0}; MT8167_USB20_IO io={&m,rd8,rd16,wr8,wr16}; MT8167_USB20_EP0 e;
    unsigned char req[8]={0x80,6,0,1,0,0,10,0}, data[10]={0};
    assert(Mt8167Ep0Start(&e,&io,req,data,10,8)==EP0_SEND_SETUP);
    assert(m.outpos==8 && !memcmp(m.out,req,8) && m.csr==0x80a);
    assert(Mt8167Ep0Interrupt(&e)==EP0_WAIT);
    m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN && m.csr==0x20);
    m.csr=1; m.count=8; memset(m.in,0x42,8);
    assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN && data[7]==0x42);
    m.csr=1; m.count=2; m.in[8]=0x31; m.in[9]=0x32;
    assert(Mt8167Ep0Interrupt(&e)==EP0_STATUS_OUT && m.csr==0x42);
    assert(data[8]==0x31 && data[9]==0x32);
    m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_COMPLETE);
    assert(Mt8167Ep0Start(&e,&io,req,data,10,8)==EP0_SEND_SETUP);
    m.csr=4; assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT && e.transfer.stage==EP0_FAILED);
    { unsigned char send[8]={0,1,0,0,0,0,10,0}; unsigned char payload[10]={0,1,2,3,4,5,6,7,8,9};
      m.outpos=0; assert(Mt8167Ep0Start(&e,&io,send,payload,10,8)==EP0_SEND_SETUP);
      m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_SEND_OUT && m.outpos==16);
      assert(!memcmp(m.out+8,payload,8));
      m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_SEND_OUT && m.outpos==18);
      assert(!memcmp(m.out+16,payload+8,2));
      m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_STATUS_IN && m.csr==0x60);
      m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_COMPLETE);
    }
    assert(Mt8167Ep0Start(&e,&io,req,data,10,8)==EP0_SEND_SETUP);
    m.csr=0; assert(Mt8167Ep0Interrupt(&e)==EP0_REQUEST_IN);
    m.csr=1; m.count=9; assert(Mt8167Ep0Interrupt(&e)==EP0_ABORT);
    return 0;
}

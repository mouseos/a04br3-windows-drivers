#include <assert.h>
#include "ep0.h"
int main(void) {
    EP0_TRANSFER t;
    unsigned char get[8] = {0x80,6,0,1,0,0,18,0};
    unsigned char set[8] = {0,9,1,0,0,0,0,0};
    unsigned char out[8] = {0,1,0,0,0,0,10,0};
    assert(Ep0Begin(&t,get,18,8)==EP0_SEND_SETUP);
    assert(Ep0Advance(&t,0)==EP0_REQUEST_IN);
    assert(Ep0Advance(&t,8)==EP0_REQUEST_IN);
    assert(Ep0Advance(&t,8)==EP0_REQUEST_IN);
    assert(Ep0Advance(&t,2)==EP0_STATUS_OUT && t.actual==18);
    assert(Ep0Advance(&t,0)==EP0_COMPLETE);
    assert(Ep0Begin(&t,get,18,8)==EP0_SEND_SETUP);
    assert(Ep0Advance(&t,0)==EP0_REQUEST_IN);
    assert(Ep0Advance(&t,3)==EP0_STATUS_OUT && t.actual==3);
    assert(Ep0Begin(&t,set,0,8)==EP0_SEND_SETUP);
    assert(Ep0Advance(&t,0)==EP0_STATUS_IN);
    assert(Ep0Advance(&t,0)==EP0_COMPLETE);
    assert(Ep0Begin(&t,out,10,8)==EP0_SEND_SETUP);
    assert(Ep0Advance(&t,0)==EP0_SEND_OUT && t.pending==8);
    assert(Ep0Advance(&t,8)==EP0_SEND_OUT && t.pending==2);
    assert(Ep0Advance(&t,2)==EP0_STATUS_IN);
    assert(Ep0Advance(&t,0)==EP0_COMPLETE);
    assert(Ep0Begin(&t,get,17,8)==EP0_ABORT);
    assert(Ep0Begin(&t,get,18,7)==EP0_ABORT);
    assert(Ep0Begin(&t,get,18,8)==EP0_SEND_SETUP);
    assert(Ep0Advance(&t,0)==EP0_REQUEST_IN);
    assert(Ep0Advance(&t,9)==EP0_ABORT);
    return 0;
}

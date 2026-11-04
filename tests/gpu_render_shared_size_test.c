/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_shared_size.h"
int main(void)
{
    uint8_t state[16432]={0};struct a04_render_shared_size_result r={0},old;unsigned n;
    for (n=0;n<=300;++n) {
        state[0]=(uint8_t)n;state[1]=(uint8_t)(n>>8);old=r;
        if (n<=256 && !(n&63)) {
            assert(!a04_render_check_saved_shared_size(state,sizeof(state),&r));
            assert(r.blocks==n && r.dwords==16*n && r.bytes==64*n);
        } else assert(a04_render_check_saved_shared_size(state,sizeof(state),&r) && !memcmp(&r,&old,sizeof(r)));
    }
    state[2]=1;old=r;
    assert(a04_render_check_saved_shared_size(state,sizeof(state),&r) && !memcmp(&r,&old,sizeof(r)));
    assert(a04_render_check_saved_shared_size(state,16431,&r));
    assert(a04_render_check_saved_shared_size(0,sizeof(state),&r));
    assert(a04_render_check_saved_shared_size(state,sizeof(state),0));
    return 0;
}

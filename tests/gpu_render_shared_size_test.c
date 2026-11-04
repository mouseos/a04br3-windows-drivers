/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "render_shared_size.h"
#include "render_trace_shared.h"
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
    {
        uint32_t trace[16]={0}; struct a04_shared_trace_summary summary={0};
        /* A packet wraps around the ring; arbitrary large words are counted
         * but never copied into the result. */
        trace[14]=A04_SHARED_TRACE_ID; trace[1]=64; trace[2]=256;
        trace[6]=A04_SHARED_TRACE_ID; trace[9]=0xdeadbeef; trace[10]=128;
        a04_shared_trace_scan(trace,16,&summary);
        assert(summary.candidates==2 && summary.store_mask==2 && summary.load_mask==20);
        assert(summary.other==1 && summary.different==2);
        a04_shared_trace_scan(trace,0,&summary);
        assert(summary.candidates==2);
    }
    return 0;
}

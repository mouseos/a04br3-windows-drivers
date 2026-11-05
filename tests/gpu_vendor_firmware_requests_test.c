/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "A04BR3RogueFirmwareRequests.h"

struct fixture { unsigned calls,barriers; int reject; };
static void barrier(void *p) { ((struct fixture *)p)->barriers++; }
static int handle(void *p,const RGXFWIF_FWCCB_CMD *cmd)
{
    struct fixture *f=p; f->calls++;
    assert(cmd->eCmdType==RGXFWIF_FWCCB_CMD_DEBUG_DUMP);
    return f->reject;
}
int main(void)
{
    struct a04_rgx_fw_runtime r={0};struct fixture f={0};
    struct a04_rgx_request_io io={&f,barrier,barrier,handle};
    RGXFWIF_INIT init={0};RGXFWIF_CCB_CTL *ctl;
    RGXFWIF_FWCCB_CMD *ring;uint32_t done;
    r.main.cpu=calloc(1,A04_RGX_MAIN_BYTES);assert(r.main.cpu);
    r.main.bytes=A04_RGX_MAIN_BYTES;r.main.used=4096;r.main.heap_offset=A04_RGX_MAIN_OFFSET;
    r.init=&init;ctl=(void *)r.main.cpu;ring=(void *)(r.main.cpu+64);r.fctl=ctl;
    init.psFirmwareCCBCtl.ui32Addr=0xc0000000u+A04_RGX_MAIN_OFFSET;
    init.psFirmwareCCB.ui32Addr=init.psFirmwareCCBCtl.ui32Addr+64;
    ctl->ui32WrapMask=31;ctl->ui32CmdSize=sizeof(*ring);
    ctl->ui32ReadOffset=31;ctl->ui32WriteOffset=1;
    ring[31].eCmdType=ring[0].eCmdType=RGXFWIF_FWCCB_CMD_DEBUG_DUMP;
    f.reject=1;assert(a04_rgx_firmware_requests(&r,&io,2,&done)==A04_REQUEST_BLOCKED);
    assert(done==0 && ctl->ui32ReadOffset==31);
    f.reject=0;assert(a04_rgx_firmware_requests(&r,&io,1,&done)==A04_REQUEST_MORE);
    assert(done==1 && ctl->ui32ReadOffset==0);
    assert(a04_rgx_firmware_requests(&r,&io,2,&done)==A04_REQUEST_EMPTY);
    assert(done==1 && ctl->ui32ReadOffset==1 && f.calls==3);
    ctl->ui32WriteOffset=2;ring[1].eCmdType=RGXFWIF_FWCCB_CMD_FREELIST_GROW;
    assert(a04_rgx_firmware_requests(&r,&io,1,&done)==A04_REQUEST_BLOCKED);
    assert(done==0 && ctl->ui32ReadOffset==1 && f.calls==3);
    ctl->ui32WriteOffset=2;ring[1].eCmdType=0;
    assert(a04_rgx_firmware_requests(&r,&io,1,&done)==A04_REQUEST_CORRUPT);
    assert(done==0 && ctl->ui32ReadOffset==1);
    ctl->ui32WriteOffset=32;
    assert(a04_rgx_firmware_requests(&r,&io,1,&done)==A04_REQUEST_CORRUPT);
    assert(ctl->ui32ReadOffset==1);
    free(r.main.cpu);return 0;
}

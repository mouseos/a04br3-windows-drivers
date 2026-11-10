/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../tools/gpu/dxg-global-diag-format.h"
static void put(unsigned char *p,unsigned offset,uint32_t v){for(unsigned i=0;i<4;i++)p[offset+i]=(unsigned char)(v>>(i*8));}
static unsigned emitted;
static void emit(void *context,uint32_t offset,const unsigned char *p){
 assert(context==(void *)&emitted);assert(offset==64);assert(dxg_diag_u32(p+0x3c)==0xc0000017u);emitted++;
}
int main(void){unsigned char raw[128]={0};struct dxg_diag_result r;
 put(raw,0,10);put(raw,4,64);put(raw,64,6);put(raw,68,64);put(raw,112,0x23);put(raw,124,0xc0000017u);
 r=dxg_diag_parse(raw,128,emit,&emitted);assert(!r.malformed&&!r.capacity_hit&&r.records==2&&r.tag23==1&&r.negative_tag23==1&&r.parsed_bytes==128&&emitted==1&&r.first_negative_status==0xc0000017u&&r.first_negative_offset==64);
 assert(dxg_diag_snapshot_exit(0,128,&r)==0);
 assert(dxg_diag_snapshot_exit(0xc000000du,128,&r)==3);
 assert(dxg_diag_snapshot_exit(1,128,&r)==3);
 assert(dxg_diag_snapshot_exit(0,DXG_DIAG_CAPACITY+1,&r)==5);
 put(raw,124,0);r=dxg_diag_parse(raw,128,0,0);assert(r.tag23==1&&!r.negative_tag23&&!r.malformed);
 r=dxg_diag_parse(raw,127,0,0);assert(r.malformed&&r.parsed_bytes==64&&!r.tag23);
 put(raw,68,0);r=dxg_diag_parse(raw,128,0,0);assert(r.malformed&&!r.tag23);
 put(raw,68,48);r=dxg_diag_parse(raw,112,0,0);assert(r.malformed&&!r.tag23);
 put(raw,68,65);r=dxg_diag_parse(raw,128,0,0);assert(r.malformed&&!r.tag23);
 assert(dxg_diag_snapshot_exit(0,128,&r)==5);
 r=dxg_diag_parse(0,0,0,0);assert(!r.records&&!r.tag23&&!r.malformed);
 assert(dxg_diag_snapshot_exit(0,0,&r)==4);
 r.capacity_hit=1;assert(dxg_diag_snapshot_exit(0,DXG_DIAG_CAPACITY,&r)==7);
 r=dxg_diag_parse(raw,DXG_DIAG_CAPACITY+1,0,0);assert(r.malformed);
 puts("DXG_DIAG_PARSE_PASS_RAW_BOUNDARIES_NEGATIVE_STATUS_NO_FALSE_TAG");
 return 0;
}

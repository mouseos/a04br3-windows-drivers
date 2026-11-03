/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "scsi.h"
int main(void)
{
    uint8_t out[4096], c[16]={0}; struct a04_scsi_reply r;
    c[0]=0x12;c[4]=36; r=a04_scsi_process(c,6,100,0,out,sizeof out);
    assert(r.action==A04_SCSI_REPLY && r.transfer_length==36 && !memcmp(out+8,"A04BR3",6));
    { uint8_t identity[7]={0x15,'A','J','T','D','4','R'};
      assert(a04_scsi_set_emmc_identity(out,36,identity));
      assert(!memcmp(out+8,"JEDEC15 ",8) && !memcmp(out+16,"AJTD4R",6));
      identity[2]=0; assert(!a04_scsi_set_emmc_identity(out,36,identity));
      assert(!memcmp(out+16,"AJTD4R",6));
      assert(!a04_scsi_set_emmc_identity(out,35,identity)); }
    c[1]=1;c[2]=0;c[4]=5;r=a04_scsi_process(c,6,100,0,out,sizeof out);
    assert(r.action==A04_SCSI_REPLY && out[3]==1 && out[4]==0);
    c[2]=0x83;r=a04_scsi_process(c,6,100,0,out,sizeof out);assert(r.action==A04_SCSI_ERROR && r.asc==0x24);
    memset(c,0,sizeof c); c[0]=0xa0;c[9]=16;r=a04_scsi_process(c,12,100,0,out,sizeof out);
    assert(r.action==A04_SCSI_REPLY && r.transfer_length==16 && out[3]==8 && out[8]==0);
    memset(c,0,sizeof c);c[0]=0x25;r=a04_scsi_process(c,10,100,0,out,sizeof out);
    assert(r.action==A04_SCSI_REPLY && out[3]==99 && out[6]==2);
    memset(c,0,sizeof c);c[0]=0x9e;c[1]=0x10;c[13]=32;
    r=a04_scsi_process(c,16,0x100000001ULL,0,out,sizeof out);
    assert(r.action==A04_SCSI_REPLY && out[3]==1 && out[7]==0 && out[10]==2);
    memset(c,0,sizeof c);c[0]=0x1a;c[2]=0x3f;c[4]=24;
    r=a04_scsi_process(c,6,100,0,out,sizeof out); assert(r.action==A04_SCSI_REPLY && out[2]==0x80 && out[4]==8);
    memset(c,0,sizeof c);c[0]=0x28;c[5]=99;c[8]=1;
    r=a04_scsi_process(c,10,100,0,out,sizeof out);assert(r.action==A04_SCSI_READ && r.transfer_length==512 && r.blocks.lba==99);
    c[8]=2;r=a04_scsi_process(c,10,100,0,out,sizeof out);assert(r.action==A04_SCSI_ERROR && r.asc==0x21);
    c[5]=0;c[8]=1;c[0]=0x2a;r=a04_scsi_process(c,10,100,0,out,sizeof out);
    assert(r.action==A04_SCSI_ERROR && r.sense_key==7 && r.asc==0x27);
    r=a04_scsi_process(c,10,100,1,out,sizeof out);assert(r.action==A04_SCSI_WRITE);
    c[8]=9;r=a04_scsi_process(c,10,100,1,out,sizeof out);assert(r.action==A04_SCSI_ERROR);
    return 0;
}

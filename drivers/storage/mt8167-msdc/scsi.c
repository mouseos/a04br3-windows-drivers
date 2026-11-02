/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "scsi.h"
#include "scsi_capacity.h"

static uint32_t be32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3]; }
static void put32(uint8_t *p, uint32_t v)
{ p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }
static struct a04_scsi_reply error(uint8_t key, uint8_t asc)
{ struct a04_scsi_reply r = {0}; r.action=A04_SCSI_ERROR; r.sense_key=key; r.asc=asc; return r; }
static struct a04_scsi_reply reply(const uint8_t *src, size_t n, uint32_t alloc,
                                    uint8_t *out, size_t size)
{
    struct a04_scsi_reply r = {0}; size_t i;
    if (n > alloc) n = alloc;
    if (n > size) n = size;
    if (n && !out) return error(5,0x24);
    for (i=0;i<n;++i) out[i]=src[i];
    r.action=A04_SCSI_REPLY; r.transfer_length=(uint32_t)n; return r;
}
struct a04_scsi_reply a04_scsi_process(const uint8_t *c, size_t len,
    uint64_t capacity, int writable, uint8_t *out, size_t size)
{
    static const uint8_t inquiry[36]={0,0,5,2,31,0,0,0,
        'A','0','4','B','R','3',' ',' ', 'e','M','M','C',' ','S','t','o','r','a','g','e',' ',' ',' ',' ',
        '0','0','0','1'};
    uint8_t data[32]={0}; uint32_t alloc; struct a04_scsi_reply r={0};
    if (!c || !len || !capacity) return error(5,0x24);
    switch (c[0]) {
    case 0x00: case 0x1b: case 0x1e: /* TUR, START STOP, PREVENT/ALLOW */
        if (len<6 || c[1] || (c[0]==0x1b && (c[4]&~1u)) ||
            (c[0]==0x1e && (c[4]&~1u))) return error(5,0x24);
        r.action=A04_SCSI_REPLY; return r;
    case 0x03: /* REQUEST SENSE, fixed NO SENSE */
        if (len<6 || c[1]) return error(5,0x24);
        data[0]=0x70; data[7]=10; return reply(data,18,c[4],out,size);
    case 0x12: /* INQUIRY */
        if (len<6 || (c[1]&~1u)) return error(5,0x24);
        if (!(c[1]&1u)) {
            if (c[2]) return error(5,0x24);
            return reply(inquiry,sizeof inquiry,((uint32_t)c[3]<<8)|c[4],out,size);
        }
        if (c[2]!=0) return error(5,0x24);
        data[1]=0; data[3]=1; data[4]=0; return reply(data,5,((uint32_t)c[3]<<8)|c[4],out,size);
    case 0xa0: /* REPORT LUNS */
        if (len<12 || c[2] || c[3] || c[4] || c[5]) return error(5,0x24);
        put32(data,8); alloc=be32(c+6); return reply(data,16,alloc,out,size);
    case 0x25: /* READ CAPACITY(10), PMI unsupported */
        if (len<10 || c[1] || be32(c+2) || c[6] || c[7] || c[8] || size<8 || !out)
            return error(5,0x24);
        if (!a04_scsi_capacity10(capacity,data,sizeof data)) return error(5,0x24);
        return reply(data,8,8,out,size);
    case 0x9e: /* READ CAPACITY(16) */
        if (len<16 || c[1]!=0x10 || be32(c+2) || be32(c+6) || (c[14]&1)) return error(5,0x24);
        put32(data,(uint32_t)((capacity-1)>>32)); put32(data+4,(uint32_t)(capacity-1));
        put32(data+8,512); alloc=be32(c+10); return reply(data,32,alloc,out,size);
    case 0x1a: case 0x5a: { /* MODE SENSE: caching page, write-protect bit */
        int ten=c[0]==0x5a; size_t header=ten?8:4, n=header;
        uint8_t page;
        if (len<(ten?10u:6u) || (c[1]&~8u) || (c[2]&0xc0u) || c[3]) return error(5,0x24);
        page=c[2]&0x3f;
        if (page!=0x3f && page!=0x08 && page!=0) return error(5,0x24);
        if (page==0x3f || page==0x08) { data[header]=8; data[header+1]=18; n+=20; }
        if (ten) { data[1]=(uint8_t)(n-2); if (!writable) data[3]=0x80; alloc=((uint32_t)c[7]<<8)|c[8]; }
        else { data[0]=(uint8_t)(n-1); if (!writable) data[2]=0x80; alloc=c[4]; }
        return reply(data,n,alloc,out,size);
    }
    case 0x35: case 0x91: /* cache disabled: SYNCHRONIZE CACHE */
        if (len<(c[0]==0x35?10u:16u) || c[1]) return error(5,0x24);
        r.action=A04_SCSI_REPLY; return r;
    case 0x28: case 0x2a: case 0x88: case 0x8a: {
        enum a04_cdb_status status=a04_decode_block_cdb(c,len,capacity,&r.blocks);
        if (status==A04_CDB_OUT_OF_RANGE) return error(5,0x21);
        if (status!=A04_CDB_OK || c[1] || r.blocks.block_count>8) return error(5,0x24);
        if (r.blocks.write && !writable) return error(7,0x27);
        if ((uint64_t)r.blocks.block_count*512u>size || (r.blocks.block_count && !out)) return error(5,0x24);
        r.action=r.blocks.write?A04_SCSI_WRITE:A04_SCSI_READ;
        r.transfer_length=r.blocks.block_count*512u; return r;
    }
    default: return error(5,0x20);
    }
}

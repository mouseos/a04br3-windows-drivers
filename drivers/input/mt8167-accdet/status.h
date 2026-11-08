/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Additional diagnostic ABI. Existing socket16/schema1 remains unchanged.
 * Values are masked to GPIO19 fields; no unrelated GPIO/pad state is exposed. */
#ifndef MT8167_ACCDET_STATUS_H
#define MT8167_ACCDET_STATUS_H
#include "socket.h"
#define MT8167_ACCDET_REASON_DIR       (1u<<0)
#define MT8167_ACCDET_REASON_MODE      (1u<<1)
#define MT8167_ACCDET_REASON_IES       (1u<<2)
#define MT8167_ACCDET_REASON_PULL      (1u<<3)
#define MT8167_ACCDET_REASON_SELECT    (1u<<4)
#define MT8167_ACCDET_REASON_CHANGED   (1u<<5)
#define MT8167_ACCDET_REASON_IO        (1u<<6)
#define MT8167_ACCDET_REASON_ADMISSION (1u<<7)
#define MT8167_ACCDET_REASON_MAPPING   (1u<<8)
#define MT8167_ACCDET_REASON_OFFLINE   (1u<<9)
#define MT8167_ACCDET_REASON_POISON    (1u<<10)
#define MT8167_ACCDET_REASON_REMOVED   (1u<<11)
#define MT8167_ACCDET_REASON_LEASE     (1u<<12)
struct mt8167_accdet_status {
    uint32_t size,version,result,reasons,valid;
    uint32_t first[6],last[6]; /* DIR,MODE,IES,PULLEN,PULLSEL,DIN */
};
static inline void mt8167_accdet_status_init(struct mt8167_accdet_status *s)
{
    unsigned i;s->size=sizeof(*s);s->version=1;s->result=MT8167_ACCDET_NOT_READY;
    s->reasons=0;s->valid=0;for(i=0;i<6;i++){s->first[i]=0;s->last[i]=0;}
}
static inline void mt8167_accdet_read_status(const struct mt8167_accdet_io *io,struct mt8167_accdet_status *s)
{
    static const unsigned offsets[6]={0x010,0x330,0x910,0x510,0x610,0x210};
    static const uint32_t masks[6]={1u<<3,7u<<12,1u<<10,1u<<3,1u<<3,1u<<3};
    unsigned i,pass;uint32_t value;
    mt8167_accdet_status_init(s);
    if(!io||!io->read32){s->result=MT8167_ACCDET_INVALID;s->reasons=MT8167_ACCDET_REASON_IO;return;}
    for(pass=0;pass<2;pass++)for(i=0;i<6;i++){
        value=0;
        if(io->read32(io->context,offsets[i],&value)){s->reasons|=MT8167_ACCDET_REASON_IO;continue;}
        if(pass)s->last[i]=value&masks[i];else s->first[i]=value&masks[i];
        s->valid|=1u<<(pass*6+i);
    }
    if(s->reasons&MT8167_ACCDET_REASON_IO){s->result=MT8167_ACCDET_IO;return;}
    if(s->first[0])s->reasons|=MT8167_ACCDET_REASON_DIR;
    if(s->first[1])s->reasons|=MT8167_ACCDET_REASON_MODE;
    if(!s->first[2])s->reasons|=MT8167_ACCDET_REASON_IES;
    if(!s->first[3]&&s->first[4])s->reasons|=MT8167_ACCDET_REASON_PULL;
    if(s->first[3]&&!s->first[4])s->reasons|=MT8167_ACCDET_REASON_SELECT;
    for(i=0;i<6;i++)if(s->first[i]!=s->last[i])s->reasons|=MT8167_ACCDET_REASON_CHANGED;
    if(s->reasons&31u)s->result=MT8167_ACCDET_NOT_READY;
    else s->result=(s->reasons&MT8167_ACCDET_REASON_CHANGED)?MT8167_ACCDET_CHANGED:MT8167_ACCDET_OK;
}
#endif

/* SPDX-License-Identifier: MIT */
#include "admission.h"
int a04_bq_admission_prepare(struct a04_bq_owner *owner,const uint8_t *record,unsigned size,
                             uint64_t now_ns,uint64_t counter,uint64_t frequency,
                             uint64_t lifetime,uint64_t epoch)
{
    if (!owner) return -1;
    if (!epoch || !lifetime || now_ns>=lifetime) { owner->stopped=1; return -1; }
    if (a04_bq_adopt_handoff(owner,record,size,now_ns,counter,frequency)) return -1;
    if (owner->handoff_deadline>lifetime) owner->handoff_deadline=lifetime;
    if (owner->handoff_deadline<=now_ns) {owner->stopped=1;owner->handoff_pending=0;return -1;}
    return 0;
}

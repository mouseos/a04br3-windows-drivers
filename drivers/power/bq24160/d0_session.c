/* SPDX-License-Identifier: MIT */
#include "d0_session.h"
int a04_bq_d0_begin(struct a04_bq_d0_session *s)
{
    int resume;
    if (!s) return 0;
    if (s->failed || s->entered || s->generation==UINT64_MAX ||
        (s->generation && !s->reset_complete)) {s->failed=1;return 0;}
    resume=s->generation!=0;
    s->reset_complete=0;s->entered=1;++s->generation;
    return resume ? 2 : 1;
}
void a04_bq_d0_default_result(struct a04_bq_d0_session *s,int result)
{
    if (!s) return;
    if (!s->entered || s->failed || result) {s->failed=1;s->reset_complete=0;return;}
    s->entered=0;s->reset_complete=1;
}
int a04_bq_d0_can_resume(const struct a04_bq_d0_session *s)
{ return s && !s->failed && !s->entered && s->generation && s->reset_complete; }

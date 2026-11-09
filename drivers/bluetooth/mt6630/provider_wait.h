/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_PROVIDER_WAIT_H
#define A04_MT6630_PROVIDER_WAIT_H
#include <stdint.h>
#define A04_PROVIDER_WAIT_TICKS UINT64_C(300000000) /*30s of100ns interrupt time*/
#define A04_PROVIDER_WAIT_ATTEMPTS 300u
struct a04_provider_wait {uint64_t began;uint32_t attempts;};
enum {A04_PROVIDER_NAMESPACE=0,A04_PROVIDER_ADMISSION=1,A04_PROVIDER_HCI=2};
static inline int a04_provider_may_wait(const struct a04_provider_wait *w,uint32_t status,unsigned phase,uint64_t now,int child_reported)
{
 if(!w||child_reported||now<w->began||now-w->began>=A04_PROVIDER_WAIT_TICKS||w->attempts>=A04_PROVIDER_WAIT_ATTEMPTS)return 0;
 if(phase==A04_PROVIDER_NAMESPACE)return status==0xc0000034u||status==0xc000003au||status==0xc000000eu||status==0xc00000a3u;
 return phase==A04_PROVIDER_ADMISSION&&status==0xc00000a3u;
}
#endif

/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "../drivers/bluetooth/mt6630/provider_wait.h"
int main(void){
 struct a04_provider_wait w={1000,1};
 assert(a04_provider_may_wait(&w,0xc0000034u,A04_PROVIDER_NAMESPACE,1000,0));
 assert(a04_provider_may_wait(&w,0xc000003au,A04_PROVIDER_NAMESPACE,2000,0));
 assert(!a04_provider_may_wait(&w,0xc0000022u,A04_PROVIDER_NAMESPACE,2000,0));
 assert(!a04_provider_may_wait(&w,0xc0000043u,A04_PROVIDER_NAMESPACE,2000,0));
 assert(a04_provider_may_wait(&w,0xc00000a3u,A04_PROVIDER_ADMISSION,2000,0));
 assert(!a04_provider_may_wait(&w,0xc0000010u,A04_PROVIDER_ADMISSION,2000,0));
 assert(!a04_provider_may_wait(&w,0xc0000185u,A04_PROVIDER_ADMISSION,2000,0));
 assert(!a04_provider_may_wait(&w,0xc00000a3u,A04_PROVIDER_HCI,2000,0));
 assert(!a04_provider_may_wait(&w,0xc0000034u,A04_PROVIDER_NAMESPACE,2000,1));
 assert(!a04_provider_may_wait(&w,0xc0000034u,A04_PROVIDER_NAMESPACE,999,0));
 assert(!a04_provider_may_wait(&w,0xc0000034u,A04_PROVIDER_NAMESPACE,1000+A04_PROVIDER_WAIT_TICKS,0));
 w.attempts=A04_PROVIDER_WAIT_ATTEMPTS;assert(!a04_provider_may_wait(&w,0xc0000034u,A04_PROVIDER_NAMESPACE,2000,0));
 puts("MT6630 provider wait: namespace/admission only, bounded, no HCI retry, no reported-child deferral: PASS");return 0;
}

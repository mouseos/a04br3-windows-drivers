/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "identity.h"
#include "ownership.h"
int main(void) {
 const uint8_t good[]={0,0x15,1,0x42,0x20,4,0x7a,0x03,0x30,0x66,0xff};
 const uint8_t duplicate[]={0x20,4,0x7a,3,0x30,0x66,0x20,4,0x7a,3,0x30,0x66,0xff};
 uint8_t bad[]={0x20,4,0x7a,3,0x31,0x66,0xff};struct a04_sdio_identity id;struct a04_wifi_owner o={0};size_t n;
 for(n=0;n<sizeof(good);n++){assert(a04_mt6630_cis(good,n,&id)!=0);assert(!id.found);}
 assert(a04_mt6630_cis(good,sizeof(good),&id)==0&&id.vendor==0x37a&&id.device==0x6630);
 assert(a04_mt6630_cis(duplicate,sizeof(duplicate),&id)==A04_CIS_MALFORMED&&!id.found);
 assert(a04_mt6630_cis(bad,sizeof(bad),&id)==A04_CIS_MISMATCH&&id.found);
 assert(a04_wifi_admit(&o));o.host=o.gpio_clock=o.combo=o.firmware=1;assert(a04_wifi_admit(&o));o.irq=1;assert(!a04_wifi_admit(&o));assert(a04_wifi_admit(&o));a04_wifi_fault(&o);assert(o.poisoned&&!o.active);o.host=o.gpio_clock=o.combo=o.firmware=o.irq=1;assert(a04_wifi_admit(&o));
 puts("MT6630 CIS bounds, identity and fail-closed ownership: OK");return 0;
}

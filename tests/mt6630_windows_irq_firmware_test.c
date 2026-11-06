/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "windows_irq_firmware.h"
static int locked,reads,failed;
static int lock(void *p){(void)p;assert(!locked);locked=1;return 0;}
static void unlock(void *p){(void)p;assert(locked);locked=0;}
int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge *b,uint32_t *s){(void)b;assert(locked);reads++;*s=0x1234;return failed;}
NTSTATUS a04_irq_work_init(struct a04_irq_work *w,PDEVICE_OBJECT d,int (*fn)(void *),void *p){(void)w;(void)d;assert(fn==a04_irq_firmware_poll&&p);return 0;}
int main(void){struct a04_windows_session s={0};struct a04_irq_firmware f={0};struct a04_irq_work w={0};DEVICE_OBJECT d={0};f.session=&s;f.try_lock=lock;f.unlock=unlock;
 assert(!a04_irq_firmware_work_init(&w,&d,&f));assert(a04_irq_firmware_poll(&f)&&!reads&&!locked);
 s.state=A04_SESSION_READY;s.diagnostic.state=A04_DIAG_READY;failed=1;assert(a04_irq_firmware_poll(&f)&&reads==1&&!f.valid&&!locked);
 failed=0;assert(!a04_irq_firmware_poll(&f)&&f.valid&&f.observed==0x1234&&reads==2&&!locked);assert(a04_irq_firmware_poll(&f)&&reads==2);return 0;}

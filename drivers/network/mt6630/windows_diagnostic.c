/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_diagnostic.h"
int a04_windows_diagnostic_start(struct a04_windows_diagnostic *d,A04_SHARED_WIFI_INTERFACE *parent,
 struct a04_windows_msdc2 *host,struct a04_wifi_owner *owner,ULONG generation,uint32_t ocr,
 const struct a04_wifi_life_ops *ops,const uint8_t expected[32],const struct a04_wmt_startup_options *options)
{
 if(!d||d->state!=A04_DIAG_OFF||!parent||!host||!owner||!ops||!expected||!options||!ocr||
    !ops->mask_and_drain_irq||!ops->drain_io||!ops->unmap_host||!ops->release_lease)return -1;
 /* Once control initialization begins, a callback may have touched hardware.
  * Never silently retry or release an uncertain lease. */
 d->state=A04_DIAG_FAILED;
 if(a04_windows_wifi_board_init(&d->board_io,parent,host,owner,generation))return -1;
 if(a04_windows_wifi_control_open(&d->board_io,&d->board,&d->life,&d->bridge,ocr,ops,&d->card))return -1;
 d->state=A04_DIAG_CONTROL;
 if(a04_windows_firmware_boot(&d->bridge,expected,options)){d->state=A04_DIAG_FAILED;return -1;}
 d->state=A04_DIAG_READY;return 0;
}
int a04_windows_diagnostic_stop(struct a04_windows_diagnostic *d)
{
 if(!d||(d->state!=A04_DIAG_CONTROL&&d->state!=A04_DIAG_READY))return -1;
 if(a04_wifi_life_stop(&d->life)){d->state=A04_DIAG_FAILED;return -1;}
 d->state=A04_DIAG_STOPPED;return 0;
}

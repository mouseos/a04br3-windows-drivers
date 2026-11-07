/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_diagnostic.h"
#if defined(A04_WIFI_CALIBRATION_DIAGNOSTIC) && A04_WIFI_CALIBRATION_DIAGNOSTIC
#include "windows_calibration.h"
#endif
int a04_windows_diagnostic_start_traced(struct a04_windows_diagnostic *d,A04_SHARED_WIFI_INTERFACE *parent,
 struct a04_windows_msdc2 *host,struct a04_wifi_owner *owner,ULONG generation,uint32_t ocr,
 const struct a04_wifi_life_ops *ops,const uint8_t expected[32],const struct a04_wmt_startup_options *options,a04_session_trace_fn observe,void *observation)
{
 struct a04_windows_fw_snapshot snapshot={0};int result=-1;
 if(!d||d->state!=A04_DIAG_OFF||!parent||!host||!owner||!ops||!expected||!options||!ocr||
    !ops->mask_and_drain_irq||!ops->drain_io||!ops->unmap_host||!ops->release_lease)return -1;
 /* Once control initialization begins, a callback may have touched hardware.
  * Never silently retry or release an uncertain lease. */
 d->state=A04_DIAG_FAILED;
 if(a04_windows_firmware_prepare_traced(&snapshot,expected,observe,observation))return -1;
 result=a04_windows_wifi_board_init(&d->board_io,parent,host,owner,generation);
 A04_INIT_RESULT(A04_CP_DIAG_BOARD,A04_CP_BACKEND,result,0);if(result)goto failed;
 result=a04_windows_wifi_control_open(&d->board_io,&d->board,&d->life,&d->bridge,ocr,ops,&d->card);
 A04_INIT_RESULT(A04_CP_DIAG_CONTROL,A04_CP_BACKEND,result,(d->life.state&255)|((d->board.started?1u:0u)<<8)|((d->board.ready?1u:0u)<<9)|((d->board.poisoned?1u:0u)<<10));if(result)goto failed;
 d->state=A04_DIAG_CONTROL;
 if(a04_windows_firmware_boot_prepared_traced(&d->bridge,&snapshot,expected,options,observe,observation)){d->state=A04_DIAG_FAILED;goto failed;}
#if defined(A04_WIFI_CALIBRATION_DIAGNOSTIC) && A04_WIFI_CALIBRATION_DIAGNOSTIC
 result=a04_windows_calibration_load(&d->bridge);
 A04_INIT_RESULT(A04_CP_DIAG_CALIBRATION,A04_CP_BACKEND,result,0);
 if(result){d->state=A04_DIAG_FAILED;goto failed;}
#endif
 d->state=A04_DIAG_READY;result=0;
 goto done;
failed:result=-1;
done:a04_windows_firmware_release(&snapshot);return result;
}
/* Preserve the original untraced API for diagnostic clients. */
int a04_windows_diagnostic_start(struct a04_windows_diagnostic *d,A04_SHARED_WIFI_INTERFACE *parent,
 struct a04_windows_msdc2 *host,struct a04_wifi_owner *owner,ULONG generation,uint32_t ocr,
 const struct a04_wifi_life_ops *ops,const uint8_t expected[32],const struct a04_wmt_startup_options *options)
{return a04_windows_diagnostic_start_traced(d,parent,host,owner,generation,ocr,ops,expected,options,NULL,NULL);}
int a04_windows_diagnostic_stop(struct a04_windows_diagnostic *d)
{
 /* A firmware or bridge failure can leave an initialized life with I/O or
  * a faulted transport. It still needs the normal ordered rundown. Earlier
  * board failures have no initialized life and must not release its lease. */
 if(!d||(d->state!=A04_DIAG_CONTROL&&d->state!=A04_DIAG_READY&&
    !(d->state==A04_DIAG_FAILED && (d->life.state==A04_WIFI_LIFE_IO ||
                                     d->life.state==A04_WIFI_LIFE_FAILED))))return -1;
 if(a04_wifi_life_stop(&d->life)){d->state=A04_DIAG_FAILED;return -1;}
 d->state=A04_DIAG_STOPPED;return 0;
}

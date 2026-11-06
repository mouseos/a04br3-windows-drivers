/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_irq_firmware.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
int a04_irq_firmware_poll(void *context)
{
 struct a04_irq_firmware *f=context;uint32_t status=0;int result=-1;
 if(!f||!f->session||!f->try_lock||!f->unlock||f->valid||f->try_lock(f->context))return -1;
 if(f->session->state==A04_SESSION_READY && f->session->diagnostic.state==A04_DIAG_READY &&
    !a04_wmt_bridge_poll_wlan_status(&f->session->diagnostic.bridge,&status)) {
  f->observed=status;f->valid=1;result=0;
 }
 f->unlock(f->context);return result;
}
NTSTATUS a04_irq_firmware_work_init(struct a04_irq_work *work,PDEVICE_OBJECT device,struct a04_irq_firmware *f)
{
 if(!f||!f->session||!f->try_lock||!f->unlock||f->valid)return STATUS_INVALID_PARAMETER;
 return a04_irq_work_init(work,device,a04_irq_firmware_poll,f);
}
#endif

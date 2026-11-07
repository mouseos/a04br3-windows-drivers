/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_session_irq.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
static int connect_irq(void *p,struct a04_windows_msdc2 *host)
{struct a04_windows_session_irq *s=p;return NT_SUCCESS(a04_windows_irq_connect(&s->irq,host->base,s->raw,s->translated,a04_irq_work_notify,s->work))?0:-1;}
static int drain_irq(void *p){struct a04_windows_session_irq *s=p;return a04_windows_irq_stop(&s->irq);}
static int drain_io(void *p){struct a04_windows_session_irq *s=p;if(a04_irq_work_drain(s->work))return -1;return s->drain_io(s->context);}
static int quiesce_combo(void *p){struct a04_windows_session_irq *s=p;return s->quiesce_combo(s->context);}
int a04_windows_session_irq_bind(struct a04_windows_session_irq *s,struct a04_windows_session_platform *out)
{
 if(!s||!out||!s->raw||!s->translated||!s->work||!s->work->work||!s->drain_io||!s->quiesce_combo||s->irq.interrupt)return -1;
 out->context=s;out->connect_masked_irq=connect_irq;
 out->mask_and_drain_irq=drain_irq;out->drain_io=drain_io;out->quiesce_combo=quiesce_combo;return 0;
}
#endif

/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_session_irq.h"
#if defined(A04_WIFI_SESSION_DIAGNOSTIC) && A04_WIFI_SESSION_DIAGNOSTIC && defined(A04_WIFI_IRQ_DIAGNOSTIC) && A04_WIFI_IRQ_DIAGNOSTIC
static int pre_mask(void *p){struct a04_windows_session_irq *s=p;return s->pre_mask(s->context);}
static int connect_irq(void *p,struct a04_windows_msdc2 *host)
{struct a04_windows_session_irq *s=p;return NT_SUCCESS(a04_windows_irq_connect(&s->irq,host->base,s->raw,s->translated,s->notify,s->context))?0:-1;}
static int drain_irq(void *p){struct a04_windows_session_irq *s=p;return a04_windows_irq_stop(&s->irq);}
static int drain_io(void *p){struct a04_windows_session_irq *s=p;return s->drain_io(s->context);}
int a04_windows_session_irq_bind(struct a04_windows_session_irq *s,struct a04_windows_session_platform *out)
{
 if(!s||!out||!s->raw||!s->translated||!s->notify||!s->pre_mask||!s->drain_io||s->irq.interrupt)return -1;
 out->context=s;out->prepare_masked_irq=pre_mask;out->connect_masked_irq=connect_irq;
 out->mask_and_drain_irq=drain_irq;out->drain_io=drain_io;return 0;
}
#endif

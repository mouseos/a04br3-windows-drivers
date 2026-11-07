/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Lock-agnostic state shared by the Windows broker and portable regression.
 * The caller holds registry_lock for client state, adapter lock for requests.
 * Tokens are borrowed while the I/O manager owns the FileObject/IRP lifetime.
 * This state never proves physical BT OFF or releases a hardware lease. */
#ifndef A04_MT6630_BROKER_LIFETIME_H
#define A04_MT6630_BROKER_LIFETIME_H
#include <stddef.h>
struct a04_combo_client_file {const void *file;unsigned cleanup_seen;};
static inline int a04_combo_client_create(struct a04_combo_client_file *s,const void *file)
{if(!s||!file||s->file)return -1;s->file=file;s->cleanup_seen=0;return 0;}
static inline int a04_combo_client_controls(const struct a04_combo_client_file *s,const void *file)
{return s&&file&&s->file==file&&!s->cleanup_seen;}
/* 1: first cleanup, 0: already retiring, -1: not our accepted file. */
static inline int a04_combo_client_cleanup(struct a04_combo_client_file *s,const void *file)
{if(!s||!file||s->file!=file)return -1;if(s->cleanup_seen)return 0;s->cleanup_seen=1;return 1;}
static inline void a04_combo_client_closed(struct a04_combo_client_file *s,const void *file)
{if(s&&file&&s->file==file){s->file=NULL;s->cleanup_seen=0;}}
struct a04_combo_requests {void *normal,*cleanup;};
static inline int a04_combo_requests_put(struct a04_combo_requests *q,void *request,unsigned cleanup)
{if(!q||!request)return -1;void **slot=cleanup?&q->cleanup:&q->normal;if(*slot)return -1;*slot=request;return 0;}
/* Drain accepted operations before file cleanup; no new operations may enter
 * once client_cleanup has revoked admission under the registry lock. */
static inline void *a04_combo_requests_take(struct a04_combo_requests *q,unsigned *cleanup)
{void *p;*cleanup=0;if((p=q->normal)!=NULL)q->normal=NULL;else if((p=q->cleanup)!=NULL){q->cleanup=NULL;*cleanup=1;}return p;}
static inline int a04_combo_requests_pending(const struct a04_combo_requests *q)
{return q&&(q->normal||q->cleanup);}
#endif

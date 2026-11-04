/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "register_owner.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
struct fake {
    pthread_mutex_t owner, control;
    pthread_cond_t changed;
    int entered, release, stopped, writes;
    struct a04_register_owner regs;
};
static int lock(void *p) { return pthread_mutex_lock(&((struct fake *)p)->owner); }
static void unlock(void *p) { assert(!pthread_mutex_unlock(&((struct fake *)p)->owner)); }
static int read32(void *p,enum a04_shared_bank bank,unsigned off,uint32_t *out)
{
    struct fake *f=p; (void)bank; (void)off;
    assert(!pthread_mutex_lock(&f->control));
    f->entered=1; pthread_cond_broadcast(&f->changed);
    while (!f->release) pthread_cond_wait(&f->changed,&f->control);
    assert(!pthread_mutex_unlock(&f->control)); *out=0; return 0;
}
static int write32(void *p,enum a04_shared_bank b,unsigned off,uint32_t v)
{ struct fake *f=p; (void)b;(void)off;(void)v; ++f->writes;return 0; }
static void *reader(void *p)
{ struct fake *f=p;int down=0;assert(!a04_register_touch_input(&f->regs,&down));assert(down==1);return NULL; }
static void *stopper(void *p)
{
    struct fake *f=p; assert(!a04_register_owner_stop(&f->regs));
    assert(!pthread_mutex_lock(&f->control));f->stopped=1;
    pthread_cond_broadcast(&f->changed);assert(!pthread_mutex_unlock(&f->control));return NULL;
}
int main(void)
{
    struct fake f={0};pthread_t a,b;int down=7;
    assert(!pthread_mutex_init(&f.owner,NULL));assert(!pthread_mutex_init(&f.control,NULL));
    assert(!pthread_cond_init(&f.changed,NULL));
    f.regs.context=&f;f.regs.lock=lock;f.regs.unlock=unlock;f.regs.read32=read32;f.regs.write32=write32;
    assert(!a04_register_owner_start(&f.regs));assert(!pthread_create(&a,NULL,reader,&f));
    assert(!pthread_mutex_lock(&f.control));while(!f.entered)pthread_cond_wait(&f.changed,&f.control);
    assert(!pthread_create(&b,NULL,stopper,&f));
    /* The owner lock remains held by the in-flight callback here. */
    assert(pthread_mutex_trylock(&f.owner)!=0);assert(!f.stopped);
    f.release=1;pthread_cond_broadcast(&f.changed);assert(!pthread_mutex_unlock(&f.control));
    assert(!pthread_join(a,NULL));assert(!pthread_join(b,NULL));assert(f.stopped);
    assert(a04_register_touch_input(&f.regs,&down)!=0 && down==7);
    assert(a04_register_touch_write(&f.regs,A04_SHARED_TOP,0x84,1u<<3)!=0 && !f.writes);
    assert(!pthread_cond_destroy(&f.changed));assert(!pthread_mutex_destroy(&f.control));assert(!pthread_mutex_destroy(&f.owner));
    return 0;
}

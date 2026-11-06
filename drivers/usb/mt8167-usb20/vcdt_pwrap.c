/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "vcdt_pwrap.h"
#define INIT_DONE2 0x9cu
#define WACS2_CMD 0xa0u
#define WACS2_RDATA 0xa4u
#define WACS2_VLDCLR 0xa8u
static int wait_state(struct a04_pmic_read *b,unsigned state,uint32_t *v)
{
 unsigned i;for(i=0;i<b->attempts;i++) {
  *v=b->read32(b->context,WACS2_RDATA);
  if(((*v>>16)&7u)==state)return 0;
  b->delay(b->context);
 }
 b->poisoned=1;return -1;
}
static int adc_claim(void *p){A04_USB_VCDT_PWRAP*a=p;return a->claim(a->claim_context);}
static int adc_release(void *p){A04_USB_VCDT_PWRAP*a=p;return a->release(a->claim_context);}
static void adc_delay(void *p,unsigned us){A04_USB_VCDT_PWRAP*a=p;a->delay_us(a->claim_context,us);}
static int adc_read(void *p,unsigned reg,unsigned short *out)
{
 A04_USB_VCDT_PWRAP*a=p;struct a04_pmic_read*b=&a->owner->bus;uint32_t v;unsigned short result;
 if(reg!=0x0704u || !out || b->poisoned || !(b->read32(b->context,INIT_DONE2)&1u) || wait_state(b,0,&v))return -1;
 b->write32(b->context,WACS2_CMD,(reg>>1)<<16);
 if(wait_state(b,6,&v))return -1;
 result=(unsigned short)v;b->write32(b->context,WACS2_VLDCLR,1);
 if(wait_state(b,0,&v))return -1;
 *out=result;return 0;
}
static int adc_write(void *p,unsigned reg,unsigned short val)
{
 A04_USB_VCDT_PWRAP*a=p;struct a04_pmic_read*b=&a->owner->bus;uint32_t v;
 if(reg!=0x0734u || val!=4u || b->poisoned || !(b->read32(b->context,INIT_DONE2)&1u) || wait_state(b,0,&v))return -1;
 b->write32(b->context,WACS2_CMD,0x80000000u|((reg>>1)<<16)|val);
 return wait_state(b,0,&v);
}
int A04UsbVcdtPwrapSample(A04_USB_VCDT_PWRAP *a,A04_MT6392_VCDT_SAMPLE *sample)
{
 struct a04_pwrap_owner *o;int result=-1;
 if(!a || !sample || !(o=a->owner) || !o->lock || !o->unlock || !a->claim || !a->release || !a->delay_us)return -1;
 if(o->lock(o->lock_context))return -1;
 if(a->adc.held || a->adc.uncertain || !o->online || o->bus.poisoned || !o->bus.read32 || !o->bus.write32 || !o->bus.delay || !o->bus.attempts)goto done;
 a->adc.context=a;a->adc.acquire=adc_claim;a->adc.release=adc_release;
 a->adc.read16=adc_read;a->adc.write16=adc_write;a->adc.delay_us=adc_delay;
 result=A04Mt6392VcdtRead(&a->adc,sample);
done:o->unlock(o->lock_context);return result;
}

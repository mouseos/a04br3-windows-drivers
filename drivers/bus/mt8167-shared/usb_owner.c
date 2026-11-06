/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "usb_owner.h"
#define USB_GATE (1u<<13)
#define USBIF_GATE (1u<<24)
#define USB78_GATE (1u<<31)
static int enter(struct a04_usb_owner *o)
{ return !o || !o->lock || !o->unlock ? -1 : o->lock(o->context); }
int a04_usb_owner_acquire(struct a04_usb_owner *o)
{
 int r=-1, gpio_attempted=0; unsigned clk1,clk2; uint16_t rail;
 if(enter(o))return -1;
 if(!o->online || o->leased || o->poisoned || !o->verify_upstream || !o->top_read || !o->top_write || !o->pmic || o->pmic->poisoned)goto done;
 if(o->probe_source && o->probe_source(o->context))goto done;
 if(o->verify_upstream(o->context) || a04_pmic_vusb_get(o->pmic,&rail))goto fail;
 /* Configure GPIO low before enabling the output. Never repurpose its mux. */
 gpio_attempted=1; if(A04Gpio22Prepare(&o->gpio))goto fail;
 if(a04_pmic_vusb_set(o->pmic,1))goto fail;
 clk1=o->top_read(o->context,0x24); clk2=o->top_read(o->context,0x3c);
 o->top_write(o->context,0x84,clk1&USB_GATE);
 o->top_write(o->context,0x9c,clk2&(USBIF_GATE|USB78_GATE));
 if((o->top_read(o->context,0x24)&USB_GATE) || (o->top_read(o->context,0x3c)&(USBIF_GATE|USB78_GATE)))goto fail;
 o->leased=1; r=0; goto done;
fail:
 /* A partial transition cannot be retried without a parent recovery. VBUS is
  * driven off if possible; retain shared clocks/rail to protect other users. */
 if(gpio_attempted)(void)A04Gpio22Vbus(&o->gpio,0);
 o->poisoned=1;
done: o->unlock(o->context); return r;
}
/* Caller holds the parent lock. Stop the board switch before releasing the
 * charger/source owner; a failed release retains both leases. */
static int off_locked(struct a04_usb_owner *o)
{
 if(A04Gpio22Vbus(&o->gpio,0)){o->poisoned=1;return -1;}
 if(o->otg_held) {
  if(!o->otg_release || o->otg_release(o->context)){o->poisoned=1;return -1;}
  o->otg_held=0;
 }
 return 0;
}
int a04_usb_owner_vbus(struct a04_usb_owner *o,int on)
{
 int r=-1,source;if(on!=0 && on!=1)return -1;if(enter(o))return -1;
 if(o->leased) {
  if(!on) r=off_locked(o);
  else if(o->online && !o->poisoned) {
   if(!o->verify_overcurrent || o->verify_overcurrent(o->context)) {
    (void)off_locked(o);o->poisoned=1;
   } else if(o->otg_held) {
    r=A04Gpio22Vbus(&o->gpio,1);if(r){(void)off_locked(o);o->poisoned=1;}
   } else if(o->otg_acquire && o->otg_release) {
    source=o->otg_acquire(o->context);
    if(source==0) {
     o->otg_held=1;r=A04Gpio22Vbus(&o->gpio,1);if(r){(void)off_locked(o);o->poisoned=1;}
    } else if(source!=-1) { o->otg_held=1;o->poisoned=1; }
   }
  }
 }
 o->unlock(o->context);return r;
}
int a04_usb_owner_health(struct a04_usb_owner *o)
{
 int r=-1;if(enter(o))return -1;
 if(o->online && o->leased && !o->poisoned && o->verify_overcurrent) {
  if(!o->verify_overcurrent(o->context))r=0;
  else { (void)off_locked(o);o->poisoned=1; }
 }
 o->unlock(o->context);return r;
}
int a04_usb_owner_release(struct a04_usb_owner *o)
{
 int r=-1;if(enter(o))return -1;
 if(o->leased) {r=off_locked(o);if(!r)o->leased=0;}
 /* Other consumers may still require the clocks and VUSB. */
 o->unlock(o->context);return r;
}
int a04_usb_owner_stop(struct a04_usb_owner *o)
{
 int r; if(enter(o))return -1; o->online=0; r=o->leased?-1:0; o->unlock(o->context); return r;
}

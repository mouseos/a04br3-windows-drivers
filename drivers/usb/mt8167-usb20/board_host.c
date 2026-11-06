#include "board_host.h"
#define DEVCTL 0x60
#define ULPI_DATA 0x74
static int valid(const A04_USB_BOARD *b)
{ return b && b->power && b->vbus && b->phy_read && b->phy_write && b->mac_read && b->mac_write && b->delay_ms; }
static void phy_update(const A04_USB_BOARD *b,unsigned reg,unsigned clear,unsigned set)
{ unsigned char v=b->phy_read(b->context,reg); b->phy_write(b->context,reg,(unsigned char)((v&~clear)|set)); }
void A04UsbHostStop(const A04_USB_BOARD *b)
{
 unsigned char v;
 if(!valid(b))return;
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 (void)b->vbus(b->context,0); (void)b->power(b->context,0);
}
int A04UsbHostPrepare(const A04_USB_BOARD *b)
{
 if(!valid(b) || !b->source_ready || !b->vbus_verified ||
    b->source_ready(b->context) || b->power(b->context,1))return -1;
 /* Restore USB function and release forced suspend after the shared parent
  * has enabled VUSB and clocks. Offsets are relative to SIF+0x800. */
 b->delay_ms(b->context,1);
 phy_update(b,0x1d,0x10,0); phy_update(b,0x6b,0x04,0);
 phy_update(b,0x6e,0x01,0); phy_update(b,0x6a,0x04,0);
 phy_update(b,0x21,0x03,0); phy_update(b,0x68,0xf4,0);
 phy_update(b,0x69,0x3c,0); phy_update(b,0x6a,0xba,0);
 phy_update(b,0x1a,0x80,0x10); phy_update(b,0x18,0x08,0x06);
 b->delay_ms(b->context,1);
 phy_update(b,0x6c,0x10,0x2e); phy_update(b,0x6d,0,0x3e);
 return 0;
}
int A04UsbHostRun(const A04_USB_BOARD *b)
{
 unsigned char v;
 if(!valid(b) || !b->source_ready || !b->vbus_verified ||
    b->source_ready(b->context))return -1;
 if(b->vbus(b->context,1)) { (void)b->vbus(b->context,0);return -1; }
 b->delay_ms(b->context,100);
 /* The Linux implementation forces PHY VBUSVALID and its status helper
  * always reports true. Neither is electrical confirmation of VBUS. */
 if(b->vbus_verified(b->context)) { (void)b->vbus(b->context,0);return -1; }
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,ULPI_DATA); b->mac_write(b->context,ULPI_DATA,(unsigned char)((v|0x80u)&~0x40u));
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v|1u));
 phy_update(b,0x6c,0x10,0x2c); phy_update(b,0x6d,0,0x3e);
 return 0;
}
int A04UsbHostStart(const A04_USB_BOARD *b)
{
 if(A04UsbHostPrepare(b))return -1;
 if(A04UsbHostRun(b)){A04UsbHostStop(b);return -1;}
 return 0;
}

/* TI BQ24160 SLUSAO0H 8.6.2: writable bits of register 01 are OTG_LOCK
 * (bit 3) and EN_NOBATOP (bit 0). The other bits are read-only status.
 * Access is made only through the already-qualified charger owner. */
static int bq_valid(const A04_BQ24160_OTG_LOCK *b)
{ return b && b->owner_safe && b->read_status && b->write_status && b->switch_off; }
int A04Bq24160OtgAcquire(A04_BQ24160_OTG_LOCK *b)
{
 unsigned char v,after;
 if(!bq_valid(b) || b->held || b->uncertain || b->owner_safe(b->context) ||
    b->switch_off(b->context) || b->read_status(b->context,&v))return -1;
 /* Never adopt a lock asserted by firmware or another consumer. */
 if(v&8u)return -1;
 b->nobat=(unsigned char)(v&1u);
 if(b->owner_safe(b->context))return -1;
 b->held=1;
 if(b->write_status(b->context,(unsigned char)(b->nobat|8u)) ||
    b->read_status(b->context,&after) || (after&9u)!=(b->nobat|8u) ||
    b->owner_safe(b->context)) { b->uncertain=1;return -2; }
 return 0;
}
int A04Bq24160OtgRelease(A04_BQ24160_OTG_LOCK *b)
{
 unsigned char v,after;
 if(!bq_valid(b) || !b->held || b->owner_safe(b->context) ||
    b->switch_off(b->context) || b->read_status(b->context,&v) ||
    (v&1u)!=b->nobat) { if(b && b->held)b->uncertain=1;return -1; }
 if(v&8u) {
  if(b->write_status(b->context,b->nobat) || b->read_status(b->context,&after) ||
     (after&9u)!=b->nobat || b->owner_safe(b->context)) {b->uncertain=1;return -1;}
 } else if(!b->uncertain) {
  /* Unexpected loss of a confirmed lock may be watchdog expiry. */
  b->uncertain=1;return -1;
 }
 b->held=0;b->uncertain=0;return 0;
}

/* Stock MT8167 PMIC AUXADC channel 2: 12-bit / 1.8 V, followed by the
 * board battery-meter divider 330:39. These are charger-input estimates,
 * not proof of the physical net feeding the USB receptacle. */
int A04Mt6392VcdtRead(A04_MT6392_VCDT *a,A04_MT6392_VCDT_SAMPLE *out)
{
 unsigned i;unsigned short reg;A04_MT6392_VCDT_SAMPLE s;
 if(!a || !out || !a->acquire || !a->release || !a->read16 || !a->write16 ||
    !a->delay_us || a->held || a->uncertain || a->acquire(a->context))return -1;
 a->held=1;
 /* 0x0734 is the RQST0 SET alias: channel 2 is bit 2. */
 if(a->write16(a->context,0x0734u,4u))goto uncertain;
 a->delay_us(a->context,100u);
 for(i=0;i<100u;i++) {
  if(a->read16(a->context,0x0704u,&reg))goto uncertain;
  if(reg&0x8000u)break;
  a->delay_us(a->context,100u);
 }
 if(i==100u)goto uncertain;
 /* Match stock driver's second read after ready; error remains ambiguous. */
 if(a->read16(a->context,0x0704u,&reg) || !(reg&0x8000u))goto uncertain;
 s.raw=reg&0x0fffu;s.adc_mv=(s.raw*1800u)/4096u;
 s.charger_mv=(369u*100u*s.adc_mv/39u)/100u;
 if(a->release(a->context))goto uncertain;
 a->held=0;*out=s;return 0;
uncertain: a->uncertain=1;return -2;
}

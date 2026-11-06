#ifndef MT8167_USB20_BOARD_HOST_H
#define MT8167_USB20_BOARD_HOST_H
/* board owns clocks, regulator and GPIO22; callbacks must coordinate shared
 * clock and power ownership and return zero only after success. */
typedef struct {
 void *context;
 int (*power)(void *,int);
 int (*vbus)(void *,int);
 unsigned char (*phy_read)(void *,unsigned);
 void (*phy_write)(void *,unsigned,unsigned char);
 unsigned char (*mac_read)(void *,unsigned);
 void (*mac_write)(void *,unsigned,unsigned char);
 void (*delay_ms)(void *,unsigned);
 /* source_ready is a side-effect-free, repeatable check by the real source
  * owner. vbus(on) must acquire and retain charger lock/watchdog and source
  * ownership before raising the switch. vbus_verified must
  * report an independent, qualified electrical observation, not GPIO22's
  * output latch, forced PHY bits, DEVCTL, or the Linux constant-true stub. */
 int (*source_ready)(void *);
 int (*vbus_verified)(void *);
} A04_USB_BOARD;
/* BQ24160 register 0x01 USB input lockout, not a 5 V boost control.
 * The caller holds the actual exclusive I2C/charger lease for the complete
 * acquire..release lifetime. owner_safe must verify that lease and continuous
 * watchdog servicing; switch_off must verify the physical switch is off.
 * Neither callback may infer these facts from GPIO output or PHY status. */
typedef struct {
 void *context;
 int (*owner_safe)(void *);
 int (*read_status)(void *,unsigned char *);
 int (*write_status)(void *,unsigned char);
 int (*switch_off)(void *);
 unsigned char nobat;
 int held,uncertain;
} A04_BQ24160_OTG_LOCK;
/* 0=locked, -1=refused without a write, -2=state uncertain; caller must
 * retain owner/watchdog resources on -2 until verified release succeeds. */
int A04Bq24160OtgAcquire(A04_BQ24160_OTG_LOCK *);
int A04Bq24160OtgRelease(A04_BQ24160_OTG_LOCK *);
/* MT6392 VCDT channel 2 charger-input observation. This is not qualified
 * as connector VBUS until the board wiring is independently established.
 * acquire must obtain real exclusive AUXADC and PWRAP ownership including
 * firmware/EL3; no ownership is inferred from a mutex alone. On -2 the
 * caller must retain resources and perform owner-specific recovery. */
typedef struct {
 void *context;
 int (*acquire)(void *);
 int (*release)(void *);
 int (*read16)(void *,unsigned,unsigned short *);
 int (*write16)(void *,unsigned,unsigned short);
 void (*delay_us)(void *,unsigned);
 int held,uncertain;
} A04_MT6392_VCDT;
typedef struct { unsigned raw,adc_mv,charger_mv; } A04_MT6392_VCDT_SAMPLE;
int A04Mt6392VcdtRead(A04_MT6392_VCDT *,A04_MT6392_VCDT_SAMPLE *);
int A04UsbHostStart(const A04_USB_BOARD *);
/* Prepare owns power and initializes the PHY. Arm the IRQ before Run so a
 * device already connected when SESSION starts cannot lose CONNECT. */
int A04UsbHostPrepare(const A04_USB_BOARD *);
int A04UsbHostRun(const A04_USB_BOARD *);
void A04UsbHostStop(const A04_USB_BOARD *);
/* connect/disconnect is reported by MUSB INTRUSB, not by DEVCTL alone. */
#endif

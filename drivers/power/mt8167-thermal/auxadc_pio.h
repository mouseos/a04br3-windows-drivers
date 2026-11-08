/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_AUXADC_PIO_H
#define A04_AUXADC_PIO_H
#include <stdint.h>
/* Qualified caller only: owns the whole ADC, stable clocks, firmware exclusion,
 * a stopped thermal AHB master, drained IRQ and owner lifetime across removal.
 * No caller in this repository currently establishes that external contract.
 * Offsets are relative to AUXADC 0x11003000; Linux uses 16-bit accesses. */
struct a04_auxadc_pio_io {
 void *context;
 int (*qualified)(void *);
 int (*read16)(void *,unsigned,uint16_t *);
 int (*write16)(void *,unsigned,uint16_t);
 int (*wait_us)(void *,unsigned);
};
struct a04_auxadc_pio_state {
 unsigned touched, quarantined;
};
enum a04_auxadc_pio_result {
 A04_PIO_OK=0, A04_PIO_REJECTED=-1,
 A04_PIO_SAMPLE_FAILED_ADC_IDLE=-2, A04_PIO_RETAIN=-3
};
/* Samples channel11 only. Does not select a sensor, invent calibration, publish
 * a temperature or authorize lease/mapping release. Caller must independently
 * prove sensor identity/analog settling and convert with its real calibration.
 * Every result leaves the caller's lease held. RETAIN forbids retry/unmap/gate
 * or ordinary resource release; it is not a request to crash/reset the OS. */
enum a04_auxadc_pio_result a04_auxadc_pio_channel11(
 struct a04_auxadc_pio_state *,const struct a04_auxadc_pio_io *,uint16_t *raw);
#endif

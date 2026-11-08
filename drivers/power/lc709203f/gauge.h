/* SPDX-License-Identifier: MIT */
#ifndef A04_LC709203F_GAUGE_H
#define A04_LC709203F_GAUGE_H
#include <stdint.h>

/* The bus owner must serialize the complete command/repeated-start/read
 * transaction with every other I2C2 client, including the BQ24160 charger. */
struct a04_gauge_bus {
    void *context;
    int (*read_word_pec)(void *context, uint8_t address, uint8_t command,
                         uint8_t response[3]);
};
struct a04_gauge_sample {
    uint16_t voltage_mv;
    uint16_t indicator_tenths_percent;
    uint8_t percent;
};
enum a04_gauge_result { A04_GAUGE_OK = 0, A04_GAUGE_IO = -1, A04_GAUGE_INVALID = -2, A04_GAUGE_UNSUPPORTED = -3 };
/* Outputs are left untouched on transaction, PEC, or range failure. */
int a04_gauge_read_sample(const struct a04_gauge_bus *bus,
                          struct a04_gauge_sample *sample);
/* Optional current IC thermistor register, in Windows 0.1 Kelvin units.
 * Same bus lock/owner as RSOC. No settings, temperature-mode or B writes.
 * Output is untouched unless both operational/thermistor checks pass. */
int a04_gauge_read_temperature(const struct a04_gauge_bus *bus, uint32_t *kelvin10);
#endif

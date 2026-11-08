/* SPDX-License-Identifier: MIT */
#include "gauge.h"

/* LC709203F PEC protocol follows A04BR3GaugeCore.h in UniXNU-a04br3,
 * MIT licensed. The stock board uses I2C2 address 0x0b. */
static uint8_t crc_byte(uint8_t crc, uint8_t value)
{
    unsigned bit;
    crc ^= value;
    for (bit = 0; bit < 8; ++bit)
        crc = (uint8_t)((crc << 1) ^ ((crc & 0x80u) ? 7u : 0u));
    return crc;
}
static int read_word(const struct a04_gauge_bus *bus, uint8_t reg, uint16_t *out)
{
    uint8_t bytes[3], crc = 0;
    if (bus->read_word_pec(bus->context, 0x0b, reg, bytes)) return -1;
    crc = crc_byte(crc, 0x16);
    crc = crc_byte(crc, reg);
    crc = crc_byte(crc, 0x17);
    crc = crc_byte(crc, bytes[0]);
    crc = crc_byte(crc, bytes[1]);
    if (crc != bytes[2]) return A04_GAUGE_INVALID;
    *out = (uint16_t)(bytes[0] | ((uint16_t)bytes[1] << 8));
    return 0;
}
int a04_gauge_read_sample(const struct a04_gauge_bus *bus,
                          struct a04_gauge_sample *sample)
{
    struct a04_gauge_sample candidate;
    uint16_t mode;
    unsigned scaled;
    int result;
    if (!bus || !bus->read_word_pec || !sample) return -1;
    result = read_word(bus, 0x15, &mode);
    if (result) return result;
    if (mode != 1) return A04_GAUGE_INVALID;
    result = read_word(bus, 0x09, &candidate.voltage_mv);
    if (result) return result;
    result = read_word(bus, 0x0f, &candidate.indicator_tenths_percent);
    if (result) return result;
    if (candidate.voltage_mv < 2500 || candidate.voltage_mv > 4500 ||
        candidate.indicator_tenths_percent > 1000) return A04_GAUGE_INVALID;
    if (candidate.indicator_tenths_percent <= 100) scaled = 0;
    else if (candidate.indicator_tenths_percent >= 950) scaled = 100;
    else scaled = ((candidate.indicator_tenths_percent - 100u) * 100u + 425u) / 850u;
    candidate.percent = (uint8_t)scaled;
    *sample = candidate;
    return 0;
}

int a04_gauge_read_temperature(const struct a04_gauge_bus *bus, uint32_t *kelvin10)
{
    uint16_t power, method, raw, after_method, after_power;
    if (!bus || !bus->read_word_pec || !kelvin10) return A04_GAUGE_INVALID;
    if (read_word(bus, 0x15, &power)) return A04_GAUGE_IO;
    if (power != 1) return A04_GAUGE_UNSUPPORTED;
    if (read_word(bus, 0x16, &method)) return A04_GAUGE_IO;
    /* I2C mode contains host-supplied data, including the default 25 C.
     * Never expose that value as a thermistor measurement. */
    if (method != 1) return A04_GAUGE_UNSUPPORTED;
    if (read_word(bus, 0x08, &raw) || read_word(bus, 0x16, &after_method) ||
        read_word(bus, 0x15, &after_power)) return A04_GAUGE_IO;
    if (after_method != method || after_power != power) return A04_GAUGE_UNSUPPORTED;
    /* LC709203F/D Rev17 p10 measurement range, not a charging limit. */
    if (raw < 0x09e4u || raw > 0x0d04u) return A04_GAUGE_INVALID;
    *kelvin10 = raw; return A04_GAUGE_OK;
}

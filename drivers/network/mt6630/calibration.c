/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "nvram.h"
/* Reader must be bounded to the discovered on-device NVRAM partition. The
 * caller allocates work outside its kernel stack and wipes it after use. */
int a04_mt6630_calibration_read(const struct a04_nvram_reader *reader,
 struct a04_nvram_work *work,uint8_t calibration[512])
{ return a04_nvram_wifi(reader,work,calibration); }

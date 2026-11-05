/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_NATIVE_COPY_USC_H
#define A04_VENDOR_NATIVE_COPY_USC_H
#include <stdint.h>
#include <stddef.h>
enum { A04_VENDOR_COPY_USC_BYTES=152 };
void a04_vendor_copy_usc_hash(const uint8_t input[152],uint8_t digest[32]);
int a04_vendor_copy_usc_match(const uint8_t *data,size_t bytes);
#endif

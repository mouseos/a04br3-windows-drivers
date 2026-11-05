/* SPDX-License-Identifier: MIT
 * Restricted m1.11 32x32, zero-colour, native format 0x22 transfer producer.
 * Derived from A04BR3RogueTransferFill.h, A04BR3RoguePixelEvent.h and
 * A04BR3RogueEOT.h at mouseos/xnu_kernel_benesse_a04br3 commit
 * f5d43bf1677955e7e367545b9bbbd6449776cdda. Those producers cite Mesa
 * e1ef8f046c6e37ec1e2a13229560fbb824e5118f and board libsrv_um evidence
 * in RENDER-UPSTREAM.json. This constructs bytes in ordinary host memory;
 * it does not submit a job or establish device completion.
 *
 * Copyright (c) 2022, 2024 Imagination Technologies Ltd.
 * Copyright (c) 2026 A04BR3 contributors.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies, subject to including this notice in all copies or substantial
 * portions. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY ARISING FROM
 * THE SOFTWARE OR ITS USE.
 */
#ifndef A04_VENDOR_CLEAR_IMAGE_H
#define A04_VENDOR_CLEAR_IMAGE_H
#include <stdint.h>
struct a04_vendor_clear_image { uint32_t payload[52],pds[16],usc[3]; };
/* Fixed target address matches the scratch mapping prepared by the driver. */
int a04_vendor_clear_image_build(struct a04_vendor_clear_image *out,uint64_t target_va);
#endif

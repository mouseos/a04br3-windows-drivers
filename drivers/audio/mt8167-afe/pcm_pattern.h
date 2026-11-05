/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_PCM_PATTERN_H
#define MT8167_PCM_PATTERN_H
#include <stdint.h>
/* Diagnostic-only 48 kHz stereo S16_LE, 1500 Hz, peak 128/32768. The caller
 * must own a private, unexposed DMA buffer and flush it before publication. */
static int mt8167_pcm_pattern(void *buffer,uint32_t bytes)
{
    static const int16_t wave[32]={0,25,49,71,91,106,118,126,128,126,118,106,91,71,49,25,
                                   0,-25,-49,-71,-91,-106,-118,-126,-128,-126,-118,-106,-91,-71,-49,-25};
    uint8_t *p=(uint8_t *)buffer;uint32_t frame;
    if(!p || !bytes || (bytes%128u))return -1;
    for(frame=0;frame<bytes/4u;frame++) {
        uint16_t v=(uint16_t)wave[frame&31u];
        p[frame*4u]=(uint8_t)v;p[frame*4u+1u]=(uint8_t)(v>>8);
        p[frame*4u+2u]=(uint8_t)v;p[frame*4u+3u]=(uint8_t)(v>>8);
    }
    return 0;
}
#endif

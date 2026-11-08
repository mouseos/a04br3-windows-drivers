/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6392_KEYS_STATE_H
#define MT6392_KEYS_STATE_H
#include "../a04br3-buttons/button_pipeline.h"
#include "../a04br3-buttons/button_state.h"
#define MT6392_KEYS_MASK ((1u<<A04_POWER)|(1u<<A04_VOLUME_DOWN))
struct mt6392_keys_state {struct a04_button_pipeline samples;uint8_t armed,failed,value;};
static inline void mt6392_keys_init(struct mt6392_keys_state *s) {
 a04_button_pipeline_init(&s->samples);s->armed=s->failed=s->value=0;
}
/* Reuse the existing three-valid-sample debounce, not the old toggle endpoint.
 * A held key at startup cannot arm. Read failure releases and latches until D0.
 * Return true when the absolute HID state changes. No register access here. */
static inline int mt6392_keys_sample(struct mt6392_keys_state *s,int valid,uint16_t raw) {
 uint8_t before=s->value;
 if(s->failed)return 0;
 if(!valid){s->failed=1;s->value=0;return before!=0;}
 (void)a04_button_pipeline_sample(&s->samples,0xffff,raw,A04_BUTTON_SOURCE_PMIC);
 if((s->samples.known&MT6392_KEYS_MASK)!=MT6392_KEYS_MASK)return 0;
 if(!s->armed){if(!(s->samples.stable&MT6392_KEYS_MASK))s->armed=1;return 0;}
 s->value=s->samples.stable&MT6392_KEYS_MASK;return before!=s->value;
}
/* Standard absolute reports: ID1 Generic Desktop/System Power Down,
 * ID2 Consumer/Volume Decrement. No toggle injection, synthetic key repeats,
 * Windows/Home or MT8167 keypad register access. */
static inline void mt6392_keys_report(const struct mt6392_keys_state *s,unsigned id,uint8_t out[2]) {
 out[0]=(uint8_t)id;out[1]=(uint8_t)((s->value&(1u<<(id==1?A04_POWER:A04_VOLUME_DOWN)))!=0);
}
static const uint8_t mt6392_keys_descriptor[]={
 0x05,0x01,0x09,0x80,0xa1,0x01,0x85,0x01,
 0x09,0x81,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0,
 0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,
 0x09,0xea,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0
};
#endif

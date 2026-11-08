/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_KEYPAD_STATE_H
#define MT8167_KEYPAD_STATE_H
#include "../a04br3-buttons/button_pipeline.h"
#include "../a04br3-buttons/button_state.h"
struct mt8167_key_domain {struct a04_button_pipeline samples;uint8_t armed,failed,value;};
struct mt8167_keypad_state {struct mt8167_key_domain pmic,keypad;};
static inline void mt8167_keypad_init(struct mt8167_keypad_state *s){
 a04_button_pipeline_init(&s->pmic.samples);a04_button_pipeline_init(&s->keypad.samples);
 s->pmic.armed=s->pmic.failed=s->pmic.value=0;
 s->keypad.armed=s->keypad.failed=s->keypad.value=0;
}
static inline void mt8167_domain_sample(struct mt8167_key_domain *s,int valid,uint16_t raw,unsigned source,uint8_t mask){
 if(s->failed)return;
 if(!valid){s->failed=1;s->value=0;return;}
 (void)a04_button_pipeline_sample(&s->samples,source==A04_BUTTON_SOURCE_KEYPAD?raw:0xffff,
 source==A04_BUTTON_SOURCE_PMIC?raw:0xffff,source);
 if((s->samples.known&mask)!=mask)return;
 if(!s->armed){if(!(s->samples.stable&mask))s->armed=1;return;}
 s->value=s->samples.stable&mask;
}
static inline uint8_t mt8167_keypad_value(const struct mt8167_keypad_state *s){return s->pmic.value|s->keypad.value;}
static inline int mt8167_keypad_sample(struct mt8167_keypad_state *s,int pv,uint16_t pmic,int kv,uint16_t keypad){
 uint8_t before=mt8167_keypad_value(s);
 mt8167_domain_sample(&s->pmic,pv,pmic,A04_BUTTON_SOURCE_PMIC,(1u<<A04_POWER)|(1u<<A04_VOLUME_DOWN));
 mt8167_domain_sample(&s->keypad,kv,keypad,A04_BUTTON_SOURCE_KEYPAD,(1u<<A04_HOME)|(1u<<A04_VOLUME_UP));
 return before!=mt8167_keypad_value(s);
}
/* Absolute two-byte reports. IDs1/2 preserve original power/volume-down;
 * ID3 Consumer Volume Increment, ID4 Keyboard Left GUI (Windows key).
 * The fourth collection is a keyboard; it is not the browser AC Home usage. */
static inline void mt8167_keypad_report(const struct mt8167_keypad_state *s,unsigned id,uint8_t out[2]){
 static const unsigned bits[4]={A04_POWER,A04_VOLUME_DOWN,A04_VOLUME_UP,A04_HOME};
 out[0]=(uint8_t)id;out[1]=(id>=1&&id<=4)?(uint8_t)((mt8167_keypad_value(s)>>bits[id-1])&1):0;
}
static const uint8_t mt8167_keypad_descriptor[]={
0x05,0x01,0x09,0x80,0xa1,0x01,0x85,0x01,
 0x09,0x81,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0,
 0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x02,
 0x09,0xea,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0,
 0x05,0x0c,0x09,0x01,0xa1,0x01,0x85,0x03,
 0x09,0xe9,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0,
 0x05,0x01,0x09,0x06,0xa1,0x01,0x85,0x04,
 0x05,0x07,0x09,0xe3,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
 0x75,0x07,0x95,0x01,0x81,0x03,0xc0
};
#endif

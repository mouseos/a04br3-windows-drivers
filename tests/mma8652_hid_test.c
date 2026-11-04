/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "sensor_hid.h"
int main(void) {
 struct a04_mma8652_sample s={-1024,0,1024}; struct a04_mma_hid_features f={1,10,20};
 uint8_t in[A04_MMA_HID_INPUT_SIZE], feat[A04_MMA_HID_FEATURE_SIZE]; size_t n;
 assert(a04_mma_hid_descriptor(&n) && n>100);
 a04_mma_hid_input(&s,in); assert(in[0]==1 && in[1]==1 && in[2]==3);
 assert(in[3]==0x9c && in[4]==0xff && in[5]==0 && in[6]==0 && in[7]==100 && in[8]==0);
 a04_mma_hid_get_feature(&f,feat); assert(feat[3]==10 && feat[5]==20 && !a04_mma_hid_set_feature(&f,feat,sizeof(feat)));
 feat[5]=19; assert(a04_mma_hid_set_feature(&f,feat,sizeof(feat)) && f.interval_ms==20);
 { struct a04_mma8652_sample prev={0,0,0}, cur={101,0,0};
   f.reporting=2; f.sensitivity=10;
   assert(!a04_mma_hid_changed(&f,&cur,&prev,1));
   cur.x=103; assert(a04_mma_hid_changed(&f,&cur,&prev,1));
   f.reporting=0; assert(!a04_mma_hid_changed(&f,&cur,&prev,0));
 }
 puts("MMA8652 sensor HID report tests passed"); return 0;
}

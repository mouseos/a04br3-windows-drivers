#include <assert.h>
#include <stdio.h>
#include "adc_observation.h"
int main(void) {
 struct a04_adc_observation x={1,sizeof(x),1,0x40000002u,0x40000002u,0};
 assert(a04_adc_observation_valid(&x,sizeof(x),6));
 assert(!a04_adc_observation_valid(&x,sizeof(x)-1,6));assert(!a04_adc_observation_valid(&x,sizeof(x),7));
 x.secure_release=1;assert(!a04_adc_observation_valid(&x,sizeof(x),6));x.secure_release=0;
 x.clock_second^=1;assert(!a04_adc_observation_valid(&x,sizeof(x),6));x.clock_first=x.clock_second=0xffffffffu;
 assert(!a04_adc_observation_valid(&x,sizeof(x),6));puts("early ADC observation never conveys ownership: OK");return 0;
}

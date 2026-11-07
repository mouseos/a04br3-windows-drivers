/* SPDX-License-Identifier: GPL-2.0-only */
#include "boot_record.h"
#include <string.h>
static int read_word(void *p,unsigned index,uint32_t *value)
{
 const struct a04_thermal_boot_cal *v=p;
 if(index==60)*value=v->d60;else if(index==61)*value=v->d61;
 else if(index==62)*value=v->d62;else return -1;
 return 0;
}
int a04_thermal_boot_calibration(struct a04_thermal_cal *out,const void *bytes,unsigned size,unsigned attributes)
{
 struct a04_thermal_boot_cal v;struct a04_thermal_cal cal;
 if(!out || !bytes || size!=sizeof(v) || attributes!=6)return -1;
 memcpy(&v,bytes,sizeof(v));
 if(v.version!=1 || a04_thermal_calibrate(&cal,read_word,&v))return -1;
 *out=cal;return 0;
}

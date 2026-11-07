/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "../hfp_volume.h"
#include <assert.h>
#include <limits.h>
int main(void)
{
 int32_t value=77;
 /* An irregular negotiated range, not a built-in headset/dB scale. */
 assert(a04_hfp_volume_select(-123,211,13,INT32_MIN,&value)&&value==-123);
 assert(a04_hfp_volume_select(-123,211,13,INT32_MAX,&value)&&value==202);
 assert(a04_hfp_volume_select(-123,211,13,7,&value)&&value==7);
 assert(a04_hfp_volume_select(-123,211,13,8,&value)&&value==7);
 assert(a04_hfp_volume_aligned(-123,211,13,202));
 assert(!a04_hfp_volume_aligned(-123,211,13,211));
 assert(a04_hfp_volume_select(INT32_MIN,INT32_MAX,UINT32_MAX,INT32_MAX,&value)&&value==INT32_MAX);
 assert(a04_hfp_volume_select(INT32_MIN,INT32_MAX,UINT32_MAX,0,&value)&&value==INT32_MIN);
 assert(a04_hfp_volume_select(INT32_MIN,INT32_MAX,1,INT32_MAX,&value)&&value==INT32_MAX);
 value=77;assert(!a04_hfp_volume_select(1,-1,13,0,&value)&&value==77);
 assert(!a04_hfp_volume_select(-1,1,0,0,&value)&&value==77);
 assert(!a04_hfp_volume_aligned(-1,1,0,0));
 return 0;
}

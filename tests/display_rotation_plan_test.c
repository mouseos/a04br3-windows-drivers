/* SPDX-License-Identifier: MIT. Pure dimensions only; no mocked OS API. */
#include <assert.h>
#include <stdio.h>
#include "../tools/display/rotation_plan.h"
int main(void){uint32_t old,requested,w,h;
 for(old=0;old<4;++old)for(requested=0;requested<4;++requested){
  assert(mt8167_rotation_size((old&1)?1280:800,(old&1)?800:1280,old,requested,&w,&h));
  assert(w==((requested&1)?1280u:800u)&&h==((requested&1)?800u:1280u));
 }
 assert(!mt8167_rotation_size(0,1280,0,0,&w,&h));
 assert(!mt8167_rotation_size(800,1280,4,0,&w,&h));
 assert(!mt8167_rotation_size(800,1280,0,4,&w,&h));
 assert(!mt8167_rotation_size(UINT32_MAX,1280,0,0,&w,&h));
 puts("PASS: real pure size planner, every original/requested enum and invalid bounds; no Windows API execution");return 0;
}

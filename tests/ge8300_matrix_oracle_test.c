/* SPDX-License-Identifier: MIT. CPU oracle validation only; no GPU success. */
#include <assert.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/wddm-render/testclient/matrix_oracle.h"
int main(void){
 for(unsigned n=0;n<sizeof(geo_matrix_cases)/sizeof(geo_matrix_cases[0]);n++){
  const struct geo_matrix_case *c=&geo_matrix_cases[n];unsigned inside=0;
  /* Validate expected bounds independently from the actual CB rows. */
  assert(400.0f*(1+c->rows[3]-c->rows[0])==c->left);
  assert(300.0f*(1-c->rows[7]-c->rows[5])==c->top);
  assert(800.0f*c->rows[0]==c->width&&600.0f*c->rows[5]==c->height);
  for(unsigned y=0;y<600;y++)for(unsigned x=0;x<800;x++)inside+=geo_matrix_pixel_inside(c,x,y);
  assert(inside&&inside<480000&&inside==c->width*c->height/2);printf("matrix case%u expected inside%u outside%u\n",n,inside,480000-inside);
 }
}

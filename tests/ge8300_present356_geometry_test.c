#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "native_clear_job.h"
#include "native_clear_surface.h"
#include "native_triangle_suite.h"
#include "frame_geometry356.h"
int main(void){
 struct a04_m111_clear_job_input in={0x8200000000ULL,0x65c0200000ULL,0xba00020000ULL,0xa800020000ULL,0x65c0202000ULL,0x65c0203000ULL,0xc0084800,0xc0084c00,{255,0,255,255}};
 struct a04_m111_clear_job a,b,unchanged;
 assert(!a04_m111_build_clear_job_800x600_cold(&a,&in));assert(!a04_m111_build_clear_job_dims(&b,&in,800,600));assert(!memcmp(&a,&b,sizeof(a)));
 assert(!a04_m111_build_clear_job_dims(&a,&in,800,1280));assert(memcmp(&a,&b,sizeof(a)));unchanged=a;
 assert(a04_m111_build_clear_job_dims(&a,&in,800,1279));assert(!memcmp(&a,&unchanged,sizeof(a)));
 struct a04_m111_triangle_suite *suite=calloc(1,sizeof(*suite));void *scratch=calloc(1,sizeof(*suite));assert(suite&&scratch);
 assert(!a04_m111_build_triangle_suite_dims(suite,&in,scratch,sizeof(*suite),800,1280));
 assert(A04_NATIVE_FRAME_PAGES==1000&&A04_NATIVE_FRAME_BYTES==4096000&&A04_NATIVE_APERTURE_PAGES==4096&&A04_NATIVE_OUTPUT_PT_COUNT==8);
 free(scratch);free(suite);return 0;}

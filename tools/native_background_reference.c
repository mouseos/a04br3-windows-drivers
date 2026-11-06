/* SPDX-License-Identifier: MIT
 * Host-only PCO recipe; same compiler provenance as native_eot_reference.c.
 * Does not access hardware.
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "common/pvr_device_info.h"
#include "util/ralloc.h"
int main(void) {
 struct pvr_device_info info;assert(!pvr_device_info_init(&info,PVR_BVNC_PACK(22,40,54,30)));
 struct pvr_device_runtime_info runtime={0};
 pco_ctx *ctx=pco_ctx_create(&info,&runtime,NULL);assert(ctx);
 nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,pco_nir_options(),"native-bg-clear-packed32");
 nir_def *colour=nir_load_preamble(&b,1,32,.base=0);
 nir_frag_store_pco(&b,colour,.base=0);nir_jump(&b,nir_jump_return);
 pco_data data={0};data.common.shareds=2;
 pco_preprocess_nir(ctx,b.shader,&data);pco_lower_nir(ctx,b.shader,&data);pco_postprocess_nir(ctx,b.shader,&data);
 pco_shader *shader=pco_trans_nir(ctx,b.shader,&data,NULL);assert(shader);
 pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
 printf("size=%u temps=%u shared=%u\n",pco_shader_binary_size(shader),pco_shader_data(shader)->common.temps,pco_shader_data(shader)->common.shareds);
 const unsigned char *code=pco_shader_binary_data(shader);
 for(unsigned j=0;j<pco_shader_binary_size(shader);j++){printf("%02x",code[j]);}puts("");
 pco_print_shader(shader,stdout,"native-bg-clear-packed32");
 ralloc_free(shader);ralloc_free(b.shader);
 ralloc_free(ctx);return 0;
}

/* SPDX-License-Identifier: MIT
 * Translated RT-to-staging point copy for source-linked native transfer.
 * Source: Mesa pvr_usc.c:pvr_uscgen_tq_frag_coords and pvr_uscgen_loadop.
 * X/Y are special hardware registers (pco_trans_nir.c:trans_nir_fs_load_input),
 * not interpolated attributes. Sampler0..3/image4..7 are owner-generated.
 * Uniform sampler/image/coordinate transform only; no embedded GPU address. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "pco/pco_internal.h"
#include "common/pvr_device_info.h"
#include "util/ralloc.h"
#include "../../../drivers/gpu/mt8167/wddm-render/compiler/pco_premultiplied.h"
int main(int argc,char **argv)
{
 assert(argc==2);
 struct pvr_device_info info;struct pvr_device_runtime_info runtime={0};pco_data data={0};
 assert(!pvr_device_info_init(&info,PVR_BVNC_PACK(22,40,54,30)));
 pco_ctx *ctx=pco_ctx_create(&info,&runtime,NULL);assert(ctx);
 nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,pco_nir_options(),"a04-premultiplied-color");
 nir_def *rgba=nir_load_preamble(&b,4,32,.base=0);
 nir_frag_store_pco(&b,geo_pco_premultiplied_bgra(&b,&data,rgba),.base=0);
 data.common.shareds=4;nir_jump(&b,nir_jump_return);
 pco_preprocess_nir(ctx,b.shader,&data);pco_lower_nir(ctx,b.shader,&data);pco_postprocess_nir(ctx,b.shader,&data);
 pco_shader *shader=pco_trans_nir(ctx,b.shader,&data,NULL);assert(shader);
 pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
 const pco_data *meta=pco_shader_data(shader);
 printf("PROGRAM bytes=%u temps=%u shared=%u z=%u w=%u\n",pco_shader_binary_size(shader),meta->common.temps,meta->common.shareds,meta->fs.uses.z,meta->fs.uses.w);
 const unsigned char *code=pco_shader_binary_data(shader);
 FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(code,1,pco_shader_binary_size(shader),f)==pco_shader_binary_size(shader));assert(!fclose(f));
 pco_print_shader(shader,stdout,"premultiplied-color");
 ralloc_free(shader);ralloc_free(b.shader);ralloc_free(ctx);return 0;
}

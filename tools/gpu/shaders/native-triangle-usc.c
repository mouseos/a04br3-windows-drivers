/* SPDX-License-Identifier: MIT
 * Source-only compiler input for BVNC22.40.54.30 native triangle.
 * No descriptor pointers or captured shader bytes occur in this source.
 * Programs: RG32 position -> UVSW xy01, shared packed colour -> pixel0,
 * shared sampler[0..3]/image[4..7] -> sample(.5,.5) -> packed pixel0.
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "pco/pco_internal.h"
#include "common/pvr_device_info.h"
#include "util/ralloc.h"
int main(void)
{
    struct pvr_device_info info;
    assert(!pvr_device_info_init(&info,PVR_BVNC_PACK(22,40,54,30)));
    struct pvr_device_runtime_info runtime={0};
    pco_ctx *ctx=pco_ctx_create(&info,&runtime,NULL);assert(ctx);
    for(unsigned test=0;test<3;test++){
        nir_builder b=nir_builder_init_simple_shader(test?MESA_SHADER_FRAGMENT:MESA_SHADER_VERTEX,
            pco_nir_options(),"a04-native-triangle");
        pco_data data={0};
        if(!test){
            nir_def *xy=nir_load_vtxin_pco(&b,2,nir_imm_int(&b,0));
            nir_uvsw_write_pco(&b,nir_imm_int(&b,0),xy);
            nir_uvsw_write_pco(&b,nir_imm_int(&b,2),nir_imm_float(&b,0));
            nir_uvsw_write_pco(&b,nir_imm_int(&b,3),nir_imm_float(&b,1));
            data.common.vtxins=2;data.vs.vtxouts=4;
        }else if(test==1){
            nir_frag_store_pco(&b,nir_load_preamble(&b,1,32,.base=0),.base=0);
            data.common.shareds=2;
        }else{
            nir_def *args[16];
            args[0]=args[1]=nir_imm_float(&b,.5);
            for(unsigned i=2;i<16;i++)args[i]=nir_undef(&b,1,32);
            /* smp_flags dim=2D; automatic LOD, normalized, no optional arguments.
             * Lowered intrinsic is source-defined in Mesa pco_nir_tex.c. */
            pco_smp_flags flags={.dim=PCO_DIM_2D,.fcnorm=true,.lod_mode=PCO_LOD_MODE_NORMAL};
            nir_def *rgba=nir_smp_pco(&b,4,nir_vec(&b,args,16),
                nir_load_preamble(&b,4,32,.base=4),
                nir_load_preamble(&b,4,32,.base=0),.smp_flags_pco=flags._,.range=2);
            nir_frag_store_pco(&b,nir_pack_unorm_4x8(&b,rgba),.base=0);
            data.common.shareds=8;
        }
        nir_jump(&b,nir_jump_return);
        pco_preprocess_nir(ctx,b.shader,&data);
        pco_lower_nir(ctx,b.shader,&data);pco_postprocess_nir(ctx,b.shader,&data);
        pco_shader *shader=pco_trans_nir(ctx,b.shader,&data,NULL);assert(shader);
        pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
        const pco_data *meta=pco_shader_data(shader);
        printf("PROGRAM %u bytes=%u temps=%u shared=%u vtxins=%u vtxouts=%u\n",
            test,pco_shader_binary_size(shader),meta->common.temps,meta->common.shareds,
            meta->common.vtxins,test?0:meta->vs.vtxouts);
        const unsigned char *code=pco_shader_binary_data(shader);
        for(unsigned i=0;i<pco_shader_binary_size(shader);i++)printf("%02x",code[i]);
        puts("");pco_print_shader(shader,stdout,"source-native-triangle");
        ralloc_free(shader);ralloc_free(b.shader);
    }
    ralloc_free(ctx);return 0;
}

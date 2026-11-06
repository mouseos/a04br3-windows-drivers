/* SPDX-License-Identifier: MIT
 * Source-generation recipe for native_eot_program.c. Build with a configured
 * Mesa PCO host compiler (source base 0d94b3f397fb9cf8470bb5600ada8af79ff5361a).
 * This does not open a GPU device. The runtime driver needs no Mesa library.
 */
#include <assert.h>
#include <stdio.h>
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "common/pvr_device_info.h"
#include "util/ralloc.h"
int main(void)
{
    struct pvr_device_info info;
    struct pvr_device_runtime_info runtime={0};
    pco_data data={.fs.uses.olchk_skip=true};
    pco_ctx *ctx;pco_shader *shader;unsigned i;
    assert(!pvr_device_info_init(&info,PVR_BVNC_PACK(22,40,54,30)));
    ctx=pco_ctx_create(&info,&runtime,NULL);assert(ctx);
    nir_builder b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,
        pco_nir_options(),"native-eot-shared");
    nir_def *a=nir_load_preamble(&b,1,32,.base=0);
    nir_def *c=nir_load_preamble(&b,1,32,.base=1);
    nir_intrinsic_instr *e=nir_emitpix_pco(&b,a,c);
    nir_intrinsic_set_freep(e,true);nir_jump(&b,nir_jump_return);
    data.common.shareds=2;
    pco_preprocess_nir(ctx,b.shader,&data);pco_lower_nir(ctx,b.shader,&data);
    pco_postprocess_nir(ctx,b.shader,&data);
    shader=pco_trans_nir(ctx,b.shader,&data,NULL);assert(shader);
    pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
    assert(pco_shader_binary_size(shader)==16);
    assert(!pco_shader_data(shader)->common.temps);
    assert(pco_shader_data(shader)->common.shareds==2);
    const unsigned char *code=pco_shader_binary_data(shader);
    for(i=0;i<16;i++)printf("%02x",code[i]);
    puts("");
    ralloc_free(shader);ralloc_free(b.shader);ralloc_free(ctx);return 0;
}

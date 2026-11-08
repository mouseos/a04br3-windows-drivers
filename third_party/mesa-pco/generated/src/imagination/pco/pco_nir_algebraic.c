#include "pco_internal.h"

#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 4
 * transforms:
 *    ('fround_even', 'a') => ('fcopysign_pco', ('bcsel', ('flt', ('bcsel', ('feq', ('ffract', ('fabs', 'a')), 0.5), ('ffract', ('fmul', ('ffloor', ('fabs', 'a')), 0.5)), ('ffract', ('fabs', 'a'))), 0.5), ('ffloor', ('fabs', 'a')), ('fadd', ('ffloor', ('fabs', 'a')), 1.0)), 'a')
 *    ('ftrunc', 'a') => ('bcsel', ('flt', 'a', 0.0), ('fceil', 'a'), ('ffloor', 'a'))
 *    ('b2b32', 'a') => ('ineg', ('b2i32', 'a'))
 *    ('b2b1', 'a') => ('ine', 'a', 0)
 */


static const nir_search_value_union pco_nir_lower_algebraic_values[] = {
   /* ('fround_even', 'a') => ('fcopysign_pco', ('bcsel', ('flt', ('bcsel', ('feq', ('ffract', ('fabs', 'a')), 0.5), ('ffract', ('fmul', ('ffloor', ('fabs', 'a')), 0.5)), ('ffract', ('fabs', 'a'))), 0.5), ('ffloor', ('fabs', 'a')), ('fadd', ('ffloor', ('fabs', 'a')), 1.0)), 'a') */
   { .variable = {
      { nir_search_value_variable, -1 },
      0, /* a */
      false,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fround_even,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace0_0_0_0_0_0_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fabs,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ffract,
      -1, 0,
      { 2 },
      -1,
   } },
   { .constant = {
      { nir_search_value_constant, -1 },
      nir_type_float, { 0x3fe0000000000000ull /* 0.5 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_feq,
      0, 1,
      { 3, 4 },
      -1,
   } },
   /* replace0_0_0_0_1_0_0_0_0 -> 0 in the cache */
   /* replace0_0_0_0_1_0_0_0 -> 2 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ffloor,
      -1, 0,
      { 2 },
      -1,
   } },
   /* replace0_0_0_0_1_0_1 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_fmul,
      1, 1,
      { 6, 4 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ffract,
      -1, 1,
      { 7 },
      -1,
   } },
   /* replace0_0_0_0_2_0_0 -> 0 in the cache */
   /* replace0_0_0_0_2_0 -> 2 in the cache */
   /* replace0_0_0_0_2 -> 3 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_bcsel,
      -1, 2,
      { 5, 8, 3 },
      -1,
   } },
   /* replace0_0_0_1 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_flt,
      -1, 2,
      { 9, 4 },
      -1,
   } },
   /* replace0_0_1_0_0 -> 0 in the cache */
   /* replace0_0_1_0 -> 2 in the cache */
   /* replace0_0_1 -> 6 in the cache */
   /* replace0_0_2_0_0_0 -> 0 in the cache */
   /* replace0_0_2_0_0 -> 2 in the cache */
   /* replace0_0_2_0 -> 6 in the cache */
   { .constant = {
      { nir_search_value_constant, -1 },
      nir_type_float, { 0x3ff0000000000000ull /* 1.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_fadd,
      2, 1,
      { 6, 11 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_bcsel,
      -1, 3,
      { 10, 6, 12 },
      -1,
   } },
   /* replace0_1 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fcopysign_pco,
      -1, 3,
      { 13, 0 },
      -1,
   } },

   /* ('ftrunc', 'a') => ('bcsel', ('flt', 'a', 0.0), ('fceil', 'a'), ('ffloor', 'a')) */
   /* search1_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ftrunc,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace1_0_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, -1 },
      nir_type_float, { 0x0ull /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_flt,
      -1, 0,
      { 0, 16 },
      -1,
   } },
   /* replace1_1_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fceil,
      -1, 0,
      { 0 },
      -1,
   } },
   /* replace1_2_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ffloor,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, -1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_bcsel,
      -1, 0,
      { 17, 18, 19 },
      -1,
   } },

   /* ('b2b32', 'a') => ('ineg', ('b2i32', 'a')) */
   /* search2_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2b32,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace2_0_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2i32,
      -1, 0,
      { 0 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ineg,
      -1, 0,
      { 22 },
      -1,
   } },

   /* ('b2b1', 'a') => ('ine', 'a', 0) */
   /* search3_0 -> 0 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2b1,
      -1, 0,
      { 0 },
      -1,
   } },

   /* replace3_0 -> 0 in the cache */
   { .constant = {
      { nir_search_value_constant, -1 },
      nir_type_int, { 0x0ull /* 0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_ine,
      0, 1,
      { 0, 25 },
      -1,
   } },

};



static const struct transform pco_nir_lower_algebraic_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 1, 14, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 15, 20, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 21, 23, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 24, 26, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table pco_nir_lower_algebraic_pass_op_table[nir_num_search_ops] = {
   [nir_op_fround_even] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         2,
      },
   },
   [nir_op_ftrunc] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         3,
      },
   },
   [nir_op_b2b32] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         4,
      },
   },
   [nir_op_b2b1] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         5,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t pco_nir_lower_algebraic_transform_offsets[] = {
   0,
   0,
   1,
   3,
   5,
   7,
};

static const nir_algebraic_table pco_nir_lower_algebraic_table = {
   .transforms = pco_nir_lower_algebraic_transforms,
   .transform_offsets = pco_nir_lower_algebraic_transform_offsets,
   .pass_op_table = pco_nir_lower_algebraic_pass_op_table,
   .values = pco_nir_lower_algebraic_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
pco_nir_lower_algebraic(
   nir_shader *shader
) {
   bool progress = false;
   bool condition_flags[1];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(27 == ARRAY_SIZE(pco_nir_lower_algebraic_values));
   condition_flags[0] = true;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &pco_nir_lower_algebraic_table);
   }

   return progress;
}


#include "nir.h"
#include "nir_builder.h"
#include "nir_search.h"
#include "nir_search_helpers.h"

/* What follows is NIR algebraic transform code for the following 14
 * transforms:
 *    ('insert_u8', 'a@32', 'b') => ('bitfield_insert', 0, 'a', ('imul', 'b', 8), 8)
 *    ('insert_u16', 'a@32', 'b') => ('bitfield_insert', 0, 'a', ('imul', 'b', 16), 16)
 *    ('extract_u8', 'a@32', 'b') => ('ubitfield_extract', 'a', ('imul', 'b', 8), 8)
 *    ('extract_i8', 'a@32', 'b') => ('ibitfield_extract', 'a', ('imul', 'b', 8), 8)
 *    ('extract_u16', 'a@32', 'b') => ('ubitfield_extract', 'a', ('imul', 'b', 16), 16)
 *    ('extract_i16', 'a@32', 'b') => ('ibitfield_extract', 'a', ('imul', 'b', 16), 16)
 *    ('b2f32', ('flt', 'a', 'b')) => ('slt', 'a', 'b@32')
 *    ('b2f32', ('fge', 'a', 'b')) => ('sge', 'a', 'b@32')
 *    ('b2f32', ('feq', 'a', 'b')) => ('seq', 'a', 'b@32')
 *    ('b2f32', ('fneu', 'a', 'b')) => ('sne', 'a', 'b@32')
 *    ('bcsel@32', ('flt', 'a', 'b'), 1.0, 0.0) => ('slt', 'a', 'b@32')
 *    ('bcsel@32', ('fge', 'a', 'b'), 1.0, 0.0) => ('sge', 'a', 'b@32')
 *    ('bcsel@32', ('feq', 'a', 'b'), 1.0, 0.0) => ('seq', 'a', 'b@32')
 *    ('bcsel@32', ('fneu', 'a', 'b'), 1.0, 0.0) => ('sne', 'a', 'b@32')
 */


static const nir_search_value_union pco_nir_lower_algebraic_late_values[] = {
   /* ('insert_u8', 'a@32', 'b') => ('bitfield_insert', 0, 'a', ('imul', 'b', 8), 8) */
   { .variable = {
      { nir_search_value_variable, 32 },
      0, /* a */
      false,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .variable = {
      { nir_search_value_variable, 32 },
      1, /* b */
      false,
      -1,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_insert_u8,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x0ull /* 0 */ },
   } },
   /* replace4_1 -> 0 in the cache */
   /* replace4_2_0 -> 1 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x8ull /* 8 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_imul,
      0, 1,
      { 1, 4 },
      -1,
   } },
   /* replace4_3 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_bitfield_insert,
      -1, 1,
      { 3, 0, 5, 4 },
      -1,
   } },

   /* ('insert_u16', 'a@32', 'b') => ('bitfield_insert', 0, 'a', ('imul', 'b', 16), 16) */
   /* search5_0 -> 0 in the cache */
   /* search5_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_insert_u16,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* replace5_0 -> 3 in the cache */
   /* replace5_1 -> 0 in the cache */
   /* replace5_2_0 -> 1 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_int, { 0x10ull /* 16 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_imul,
      0, 1,
      { 1, 8 },
      -1,
   } },
   /* replace5_3 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_bitfield_insert,
      -1, 1,
      { 3, 0, 9, 8 },
      -1,
   } },

   /* ('extract_u8', 'a@32', 'b') => ('ubitfield_extract', 'a', ('imul', 'b', 8), 8) */
   /* search6_0 -> 0 in the cache */
   /* search6_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_extract_u8,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* replace6_0 -> 0 in the cache */
   /* replace6_1_0 -> 1 in the cache */
   /* replace6_1_1 -> 4 in the cache */
   /* replace6_1 -> 5 in the cache */
   /* replace6_2 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ubitfield_extract,
      -1, 1,
      { 0, 5, 4 },
      -1,
   } },

   /* ('extract_i8', 'a@32', 'b') => ('ibitfield_extract', 'a', ('imul', 'b', 8), 8) */
   /* search7_0 -> 0 in the cache */
   /* search7_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_extract_i8,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* replace7_0 -> 0 in the cache */
   /* replace7_1_0 -> 1 in the cache */
   /* replace7_1_1 -> 4 in the cache */
   /* replace7_1 -> 5 in the cache */
   /* replace7_2 -> 4 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ibitfield_extract,
      -1, 1,
      { 0, 5, 4 },
      -1,
   } },

   /* ('extract_u16', 'a@32', 'b') => ('ubitfield_extract', 'a', ('imul', 'b', 16), 16) */
   /* search8_0 -> 0 in the cache */
   /* search8_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_extract_u16,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* replace8_0 -> 0 in the cache */
   /* replace8_1_0 -> 1 in the cache */
   /* replace8_1_1 -> 8 in the cache */
   /* replace8_1 -> 9 in the cache */
   /* replace8_2 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ubitfield_extract,
      -1, 1,
      { 0, 9, 8 },
      -1,
   } },

   /* ('extract_i16', 'a@32', 'b') => ('ibitfield_extract', 'a', ('imul', 'b', 16), 16) */
   /* search9_0 -> 0 in the cache */
   /* search9_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_extract_i16,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* replace9_0 -> 0 in the cache */
   /* replace9_1_0 -> 1 in the cache */
   /* replace9_1_1 -> 8 in the cache */
   /* replace9_1 -> 9 in the cache */
   /* replace9_2 -> 8 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_ibitfield_extract,
      -1, 1,
      { 0, 9, 8 },
      -1,
   } },

   /* ('b2f32', ('flt', 'a', 'b')) => ('slt', 'a', 'b@32') */
   /* search10_0_0 -> 0 in the cache */
   /* search10_0_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_flt,
      -1, 0,
      { 0, 1 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2f32,
      -1, 0,
      { 19 },
      -1,
   } },

   /* replace10_0 -> 0 in the cache */
   /* replace10_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_slt,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* ('b2f32', ('fge', 'a', 'b')) => ('sge', 'a', 'b@32') */
   /* search11_0_0 -> 0 in the cache */
   /* search11_0_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fge,
      -1, 0,
      { 0, 1 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2f32,
      -1, 0,
      { 22 },
      -1,
   } },

   /* replace11_0 -> 0 in the cache */
   /* replace11_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_sge,
      -1, 0,
      { 0, 1 },
      -1,
   } },

   /* ('b2f32', ('feq', 'a', 'b')) => ('seq', 'a', 'b@32') */
   /* search12_0_0 -> 0 in the cache */
   /* search12_0_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_feq,
      0, 1,
      { 0, 1 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2f32,
      -1, 1,
      { 25 },
      -1,
   } },

   /* replace12_0 -> 0 in the cache */
   /* replace12_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_seq,
      0, 1,
      { 0, 1 },
      -1,
   } },

   /* ('b2f32', ('fneu', 'a', 'b')) => ('sne', 'a', 'b@32') */
   /* search13_0_0 -> 0 in the cache */
   /* search13_0_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 1 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_fneu,
      0, 1,
      { 0, 1 },
      -1,
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_b2f32,
      -1, 1,
      { 28 },
      -1,
   } },

   /* replace13_0 -> 0 in the cache */
   /* replace13_1 -> 1 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      false,
      -1,
      nir_op_sne,
      0, 1,
      { 0, 1 },
      -1,
   } },

   /* ('bcsel@32', ('flt', 'a', 'b'), 1.0, 0.0) => ('slt', 'a', 'b@32') */
   /* search14_0_0 -> 0 in the cache */
   /* search14_0_1 -> 1 in the cache */
   /* search14_0 -> 19 in the cache */
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x3ff0000000000000ull /* 1.0 */ },
   } },
   { .constant = {
      { nir_search_value_constant, 32 },
      nir_type_float, { 0x0ull /* 0.0 */ },
   } },
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_bcsel,
      -1, 0,
      { 19, 31, 32 },
      -1,
   } },

   /* replace14_0 -> 0 in the cache */
   /* replace14_1 -> 1 in the cache */
   /* replace14 -> 21 in the cache */

   /* ('bcsel@32', ('fge', 'a', 'b'), 1.0, 0.0) => ('sge', 'a', 'b@32') */
   /* search15_0_0 -> 0 in the cache */
   /* search15_0_1 -> 1 in the cache */
   /* search15_0 -> 22 in the cache */
   /* search15_1 -> 31 in the cache */
   /* search15_2 -> 32 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_bcsel,
      -1, 0,
      { 22, 31, 32 },
      -1,
   } },

   /* replace15_0 -> 0 in the cache */
   /* replace15_1 -> 1 in the cache */
   /* replace15 -> 24 in the cache */

   /* ('bcsel@32', ('feq', 'a', 'b'), 1.0, 0.0) => ('seq', 'a', 'b@32') */
   /* search16_0_0 -> 0 in the cache */
   /* search16_0_1 -> 1 in the cache */
   /* search16_0 -> 25 in the cache */
   /* search16_1 -> 31 in the cache */
   /* search16_2 -> 32 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_bcsel,
      -1, 1,
      { 25, 31, 32 },
      -1,
   } },

   /* replace16_0 -> 0 in the cache */
   /* replace16_1 -> 1 in the cache */
   /* replace16 -> 27 in the cache */

   /* ('bcsel@32', ('fneu', 'a', 'b'), 1.0, 0.0) => ('sne', 'a', 'b@32') */
   /* search17_0_0 -> 0 in the cache */
   /* search17_0_1 -> 1 in the cache */
   /* search17_0 -> 28 in the cache */
   /* search17_1 -> 31 in the cache */
   /* search17_2 -> 32 in the cache */
   { .expression = {
      { nir_search_value_expression, 32 },
      nir_fp_fast_math,
      nir_fp_fast_math,
      true,
      -1,
      nir_op_bcsel,
      -1, 1,
      { 28, 31, 32 },
      -1,
   } },

   /* replace17_0 -> 0 in the cache */
   /* replace17_1 -> 1 in the cache */
   /* replace17 -> 30 in the cache */

};



static const struct transform pco_nir_lower_algebraic_late_transforms[] = {
   { ~0, ~0, ~0 }, /* Sentinel */

   { 2, 6, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 7, 10, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 11, 12, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 13, 14, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 15, 16, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 17, 18, 0 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 20, 21, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 23, 24, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 26, 27, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 29, 30, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 33, 21, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 34, 24, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 35, 27, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

   { 36, 30, 1 },
   { ~0, ~0, ~0 }, /* Sentinel */

};

static const struct per_op_table pco_nir_lower_algebraic_late_pass_op_table[nir_num_search_ops] = {
   [nir_op_insert_u8] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         2,
      },
   },
   [nir_op_insert_u16] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         3,
      },
   },
   [nir_op_extract_u8] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         4,
      },
   },
   [nir_op_extract_i8] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         5,
      },
   },
   [nir_op_extract_u16] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         6,
      },
   },
   [nir_op_extract_i16] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         7,
      },
   },
   [nir_search_op_b2f] = {
      .filter = (const uint16_t []) {
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         1,
         2,
         3,
         4,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 5,
      .table = (const uint16_t []) {
      
         0,
         12,
         13,
         14,
         15,
      },
   },
   [nir_op_flt] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         8,
      },
   },
   [nir_op_fge] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         9,
      },
   },
   [nir_op_feq] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         10,
      },
   },
   [nir_op_fneu] = {
      .filter = NULL,
      
      .num_filtered_states = 1,
      .table = (const uint16_t []) {
      
         11,
      },
   },
   [nir_op_bcsel] = {
      .filter = (const uint16_t []) {
         0,
         1,
         0,
         0,
         0,
         0,
         0,
         0,
         2,
         3,
         4,
         5,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
      
      .num_filtered_states = 6,
      .table = (const uint16_t []) {
      
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         16,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         17,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         18,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         19,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
         0,
      },
   },
};

/* Mapping from state index to offset in transforms (0 being no transforms) */
static const uint16_t pco_nir_lower_algebraic_late_transform_offsets[] = {
   0,
   0,
   1,
   3,
   5,
   7,
   9,
   11,
   0,
   0,
   0,
   0,
   13,
   15,
   17,
   19,
   21,
   23,
   25,
   27,
};

static const nir_algebraic_table pco_nir_lower_algebraic_late_table = {
   .transforms = pco_nir_lower_algebraic_late_transforms,
   .transform_offsets = pco_nir_lower_algebraic_late_transform_offsets,
   .pass_op_table = pco_nir_lower_algebraic_late_pass_op_table,
   .values = pco_nir_lower_algebraic_late_values,
   .expression_cond = NULL,
   .variable_cond = NULL,
};

bool
pco_nir_lower_algebraic_late(
   nir_shader *shader
) {
   bool progress = false;
   bool condition_flags[2];
   const nir_shader_compiler_options *options = shader->options;
   const shader_info *info = &shader->info;
   (void) options;
   (void) info;

   STATIC_ASSERT(37 == ARRAY_SIZE(pco_nir_lower_algebraic_late_values));
   condition_flags[0] = true;
   condition_flags[1] = !options->lower_scmp;

   nir_foreach_function_impl(impl, shader) {
     progress |= nir_algebraic_impl(impl, condition_flags, &pco_nir_lower_algebraic_late_table);
   }

   return progress;
}


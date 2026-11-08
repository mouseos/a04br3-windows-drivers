/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PCO_BUILDER_OPS_H
#define PCO_BUILDER_OPS_H

/**
 * \file pco_builder_ops.h
 *
 * \brief PCO op building functions.
 */

#include "pco_internal.h"
#include "pco_common.h"
#include "pco_ops.h"
#include "util/macros.h"

#include <assert.h>
#include <stdbool.h>

/** Ref mod info. */
static inline
bool pco_instr_dest_has_mod(const pco_instr *instr, unsigned dest, enum pco_ref_mod mod)
{
   const struct pco_op_info *info = &pco_op_info[instr->op];
   if (info->num_dests == ~0)
      return false;

   assert(dest < info->num_dests);
   assert(mod < _PCO_REF_MOD_COUNT);
   return (info->dest_mods[dest] & (1ULL << mod)) != 0;
}

static inline
bool pco_instr_src_has_mod(const pco_instr *instr, unsigned src, enum pco_ref_mod mod)
{
   const struct pco_op_info *info = &pco_op_info[instr->op];
   if (info->num_srcs == ~0)
      return false;

   assert(src < info->num_srcs);
   assert(mod < _PCO_REF_MOD_COUNT);
   return (info->src_mods[src] & (1ULL << mod)) != 0;
}

static inline
bool pco_instr_dest_has_oneminus(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_ONEMINUS);
}

static inline
bool pco_instr_src_has_oneminus(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_ONEMINUS);
}

static inline
bool pco_instr_dest_has_clamp(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_CLAMP);
}

static inline
bool pco_instr_src_has_clamp(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_CLAMP);
}

static inline
bool pco_instr_dest_has_abs(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_ABS);
}

static inline
bool pco_instr_src_has_abs(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_ABS);
}

static inline
bool pco_instr_dest_has_neg(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_NEG);
}

static inline
bool pco_instr_src_has_neg(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_NEG);
}

static inline
bool pco_instr_dest_has_flr(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_FLR);
}

static inline
bool pco_instr_src_has_flr(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_FLR);
}

static inline
bool pco_instr_dest_has_elem(const pco_instr *instr, unsigned dest)
{
   return pco_instr_dest_has_mod(instr, dest, PCO_REF_MOD_ELEM);
}

static inline
bool pco_instr_src_has_elem(const pco_instr *instr, unsigned src)
{
   return pco_instr_src_has_mod(instr, src, PCO_REF_MOD_ELEM);
}

/** Op mod getting/setting. */
static inline
bool pco_instr_has_mod(const pco_instr *instr, enum pco_op_mod mod)
{
   assert(mod < _PCO_OP_MOD_COUNT);
   return (pco_op_info[instr->op].mods & (1ULL << mod)) != 0;
}

static inline
void pco_instr_set_mod(pco_instr *instr, enum pco_op_mod mod, uint32_t val)
{
   assert(mod < _PCO_OP_MOD_COUNT);
   unsigned mod_index = pco_op_info[instr->op].mod_map[mod];
   assert(mod_index > 0);
   instr->mod[mod_index - 1] = val;
}

static inline
uint32_t pco_instr_get_mod(const pco_instr *instr, enum pco_op_mod mod)
{
   assert(mod < _PCO_OP_MOD_COUNT);
   unsigned mod_index = pco_op_info[instr->op].mod_map[mod];
   assert(mod_index > 0);
   return instr->mod[mod_index - 1];
}

static inline
bool pco_instr_mod_is_set(const pco_instr *instr, enum pco_op_mod mod)
{
   const struct pco_op_mod_info *info = &pco_op_mod_info[mod];
   return pco_instr_get_mod(instr, mod) != (info->nzdefault ? info->nzdefault : 0);
}

static inline
bool pco_instr_has_exec_cnd(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_EXEC_CND);
}

static inline
void pco_instr_set_exec_cnd(pco_instr *instr, enum pco_exec_cnd val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, val);
}

static inline
enum pco_exec_cnd pco_instr_get_exec_cnd(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND);
}

static inline
bool pco_instr_has_rpt(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_RPT);
}

static inline
void pco_instr_set_rpt(pco_instr *instr, unsigned val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_RPT, val);
}

static inline
unsigned pco_instr_get_rpt(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
}

static inline
bool pco_instr_has_sat(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SAT);
}

static inline
void pco_instr_set_sat(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SAT, val);
}

static inline
bool pco_instr_get_sat(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SAT);
}

static inline
bool pco_instr_has_lp(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_LP);
}

static inline
void pco_instr_set_lp(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_LP, val);
}

static inline
bool pco_instr_get_lp(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_LP);
}

static inline
bool pco_instr_has_scale(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SCALE);
}

static inline
void pco_instr_set_scale(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SCALE, val);
}

static inline
bool pco_instr_get_scale(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SCALE);
}

static inline
bool pco_instr_has_roundzero(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_ROUNDZERO);
}

static inline
void pco_instr_set_roundzero(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_ROUNDZERO, val);
}

static inline
bool pco_instr_get_roundzero(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_ROUNDZERO);
}

static inline
bool pco_instr_has_s(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_S);
}

static inline
void pco_instr_set_s(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_S, val);
}

static inline
bool pco_instr_get_s(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_S);
}

static inline
bool pco_instr_has_tst_type_main(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN);
}

static inline
void pco_instr_set_tst_type_main(pco_instr *instr, enum pco_tst_type_main val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN, val);
}

static inline
enum pco_tst_type_main pco_instr_get_tst_type_main(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN);
}

static inline
bool pco_instr_has_tst_op_main(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_TST_OP_MAIN);
}

static inline
void pco_instr_set_tst_op_main(pco_instr *instr, enum pco_tst_op_main val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_TST_OP_MAIN, val);
}

static inline
enum pco_tst_op_main pco_instr_get_tst_op_main(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN);
}

static inline
bool pco_instr_has_tst_op_bitwise(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_TST_OP_BITWISE);
}

static inline
void pco_instr_set_tst_op_bitwise(pco_instr *instr, enum pco_tst_op_bitwise val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_TST_OP_BITWISE, val);
}

static inline
enum pco_tst_op_bitwise pco_instr_get_tst_op_bitwise(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_BITWISE);
}

static inline
bool pco_instr_has_signpos(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SIGNPOS);
}

static inline
void pco_instr_set_signpos(pco_instr *instr, enum pco_signpos val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SIGNPOS, val);
}

static inline
enum pco_signpos pco_instr_get_signpos(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SIGNPOS);
}

static inline
bool pco_instr_has_dim(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_DIM);
}

static inline
void pco_instr_set_dim(pco_instr *instr, enum pco_dim val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_DIM, val);
}

static inline
enum pco_dim pco_instr_get_dim(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_DIM);
}

static inline
bool pco_instr_has_proj(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_PROJ);
}

static inline
void pco_instr_set_proj(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_PROJ, val);
}

static inline
bool pco_instr_get_proj(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_PROJ);
}

static inline
bool pco_instr_has_fcnorm(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_FCNORM);
}

static inline
void pco_instr_set_fcnorm(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_FCNORM, val);
}

static inline
bool pco_instr_get_fcnorm(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM);
}

static inline
bool pco_instr_has_nncoords(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_NNCOORDS);
}

static inline
void pco_instr_set_nncoords(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_NNCOORDS, val);
}

static inline
bool pco_instr_get_nncoords(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS);
}

static inline
bool pco_instr_has_lod_mode(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_LOD_MODE);
}

static inline
void pco_instr_set_lod_mode(pco_instr *instr, enum pco_lod_mode val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_LOD_MODE, val);
}

static inline
enum pco_lod_mode pco_instr_get_lod_mode(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE);
}

static inline
bool pco_instr_has_pplod(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_PPLOD);
}

static inline
void pco_instr_set_pplod(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_PPLOD, val);
}

static inline
bool pco_instr_get_pplod(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD);
}

static inline
bool pco_instr_has_tao(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_TAO);
}

static inline
void pco_instr_set_tao(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_TAO, val);
}

static inline
bool pco_instr_get_tao(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_TAO);
}

static inline
bool pco_instr_has_soo(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SOO);
}

static inline
void pco_instr_set_soo(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SOO, val);
}

static inline
bool pco_instr_get_soo(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SOO);
}

static inline
bool pco_instr_has_sno(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SNO);
}

static inline
void pco_instr_set_sno(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SNO, val);
}

static inline
bool pco_instr_get_sno(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SNO);
}

static inline
bool pco_instr_has_sb_mode(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SB_MODE);
}

static inline
void pco_instr_set_sb_mode(pco_instr *instr, enum pco_sb_mode val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SB_MODE, val);
}

static inline
enum pco_sb_mode pco_instr_get_sb_mode(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE);
}

static inline
bool pco_instr_has_array(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_ARRAY);
}

static inline
void pco_instr_set_array(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_ARRAY, val);
}

static inline
bool pco_instr_get_array(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY);
}

static inline
bool pco_instr_has_integer(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_INTEGER);
}

static inline
void pco_instr_set_integer(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_INTEGER, val);
}

static inline
bool pco_instr_get_integer(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER);
}

static inline
bool pco_instr_has_schedswap(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SCHEDSWAP);
}

static inline
void pco_instr_set_schedswap(pco_instr *instr, enum pco_schedswap val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SCHEDSWAP, val);
}

static inline
enum pco_schedswap pco_instr_get_schedswap(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP);
}

static inline
bool pco_instr_has_f16(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_F16);
}

static inline
void pco_instr_set_f16(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_F16, val);
}

static inline
bool pco_instr_get_f16(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_F16);
}

static inline
bool pco_instr_has_tiled(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_TILED);
}

static inline
void pco_instr_set_tiled(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_TILED, val);
}

static inline
bool pco_instr_get_tiled(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_TILED);
}

static inline
bool pco_instr_has_freep(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_FREEP);
}

static inline
void pco_instr_set_freep(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_FREEP, val);
}

static inline
bool pco_instr_get_freep(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_FREEP);
}

static inline
bool pco_instr_has_sm(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SM);
}

static inline
void pco_instr_set_sm(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SM, val);
}

static inline
bool pco_instr_get_sm(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SM);
}

static inline
bool pco_instr_has_savmsk_mode(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SAVMSK_MODE);
}

static inline
void pco_instr_set_savmsk_mode(pco_instr *instr, enum pco_savmsk_mode val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SAVMSK_MODE, val);
}

static inline
enum pco_savmsk_mode pco_instr_get_savmsk_mode(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SAVMSK_MODE);
}

static inline
bool pco_instr_has_atom_op(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_ATOM_OP);
}

static inline
void pco_instr_set_atom_op(pco_instr *instr, enum pco_atom_op val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_ATOM_OP, val);
}

static inline
enum pco_atom_op pco_instr_get_atom_op(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_ATOM_OP);
}

static inline
bool pco_instr_has_mcu_cache_mode_ld(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD);
}

static inline
void pco_instr_set_mcu_cache_mode_ld(pco_instr *instr, enum pco_mcu_cache_mode_ld val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD, val);
}

static inline
enum pco_mcu_cache_mode_ld pco_instr_get_mcu_cache_mode_ld(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD);
}

static inline
bool pco_instr_has_mcu_cache_mode_st(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST);
}

static inline
void pco_instr_set_mcu_cache_mode_st(pco_instr *instr, enum pco_mcu_cache_mode_st val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST, val);
}

static inline
enum pco_mcu_cache_mode_st pco_instr_get_mcu_cache_mode_st(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST);
}

static inline
bool pco_instr_has_branch_cnd(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_BRANCH_CND);
}

static inline
void pco_instr_set_branch_cnd(pco_instr *instr, enum pco_branch_cnd val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_BRANCH_CND, val);
}

static inline
enum pco_branch_cnd pco_instr_get_branch_cnd(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_BRANCH_CND);
}

static inline
bool pco_instr_has_link(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_LINK);
}

static inline
void pco_instr_set_link(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_LINK, val);
}

static inline
bool pco_instr_get_link(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_LINK);
}

static inline
bool pco_instr_has_pck_fmt(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_PCK_FMT);
}

static inline
void pco_instr_set_pck_fmt(pco_instr *instr, enum pco_pck_fmt val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_PCK_FMT, val);
}

static inline
enum pco_pck_fmt pco_instr_get_pck_fmt(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_PCK_FMT);
}

static inline
bool pco_instr_has_phase2end(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_PHASE2END);
}

static inline
void pco_instr_set_phase2end(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_PHASE2END, val);
}

static inline
bool pco_instr_get_phase2end(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END);
}

static inline
bool pco_instr_has_itr_mode(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_ITR_MODE);
}

static inline
void pco_instr_set_itr_mode(pco_instr *instr, enum pco_itr_mode val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_ITR_MODE, val);
}

static inline
enum pco_itr_mode pco_instr_get_itr_mode(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_ITR_MODE);
}

static inline
bool pco_instr_has_sched(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SCHED);
}

static inline
void pco_instr_set_sched(pco_instr *instr, enum pco_sched val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SCHED, val);
}

static inline
enum pco_sched pco_instr_get_sched(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SCHED);
}

static inline
bool pco_instr_has_atom(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_ATOM);
}

static inline
void pco_instr_set_atom(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, val);
}

static inline
bool pco_instr_get_atom(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
}

static inline
bool pco_instr_has_olchk(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_OLCHK);
}

static inline
void pco_instr_set_olchk(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, val);
}

static inline
bool pco_instr_get_olchk(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
}

static inline
bool pco_instr_has_end(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_END);
}

static inline
void pco_instr_set_end(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_END, val);
}

static inline
bool pco_instr_get_end(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_END);
}

static inline
bool pco_instr_has_logiop(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_LOGIOP);
}

static inline
void pco_instr_set_logiop(pco_instr *instr, enum pco_logiop val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_LOGIOP, val);
}

static inline
enum pco_logiop pco_instr_get_logiop(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_LOGIOP);
}

static inline
bool pco_instr_has_shiftop(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_SHIFTOP);
}

static inline
void pco_instr_set_shiftop(pco_instr *instr, enum pco_shiftop val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_SHIFTOP, val);
}

static inline
enum pco_shiftop pco_instr_get_shiftop(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_SHIFTOP);
}

static inline
bool pco_instr_has_cnd(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_CND);
}

static inline
void pco_instr_set_cnd(pco_instr *instr, enum pco_cnd val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_CND, val);
}

static inline
enum pco_cnd pco_instr_get_cnd(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_CND);
}

static inline
bool pco_instr_has_fred_type(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_FRED_TYPE);
}

static inline
void pco_instr_set_fred_type(pco_instr *instr, enum pco_fred_type val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_FRED_TYPE, val);
}

static inline
enum pco_fred_type pco_instr_get_fred_type(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_FRED_TYPE);
}

static inline
bool pco_instr_has_fred_part(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_FRED_PART);
}

static inline
void pco_instr_set_fred_part(pco_instr *instr, enum pco_fred_part val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_FRED_PART, val);
}

static inline
enum pco_fred_part pco_instr_get_fred_part(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_FRED_PART);
}

static inline
bool pco_instr_has_mutex_op(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_MUTEX_OP);
}

static inline
void pco_instr_set_mutex_op(pco_instr *instr, enum pco_mutex_op val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_MUTEX_OP, val);
}

static inline
enum pco_mutex_op pco_instr_get_mutex_op(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_MUTEX_OP);
}

static inline
bool pco_instr_has_offset_sd(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_OFFSET_SD);
}

static inline
void pco_instr_set_offset_sd(pco_instr *instr, enum pco_offset_sd val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_OFFSET_SD, val);
}

static inline
enum pco_offset_sd pco_instr_get_offset_sd(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_OFFSET_SD);
}

static inline
bool pco_instr_has_idf(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_IDF);
}

static inline
void pco_instr_set_idf(pco_instr *instr, bool val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_IDF, val);
}

static inline
bool pco_instr_get_idf(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_IDF);
}

static inline
bool pco_instr_has_vote_op(const pco_instr *instr)
{
   return pco_instr_has_mod(instr, PCO_OP_MOD_VOTE_OP);
}

static inline
void pco_instr_set_vote_op(pco_instr *instr, enum pco_vote_op val)
{
   return pco_instr_set_mod(instr, PCO_OP_MOD_VOTE_OP, val);
}

static inline
enum pco_vote_op pco_instr_get_vote_op(const pco_instr *instr)
{
   return pco_instr_get_mod(instr, PCO_OP_MOD_VOTE_OP);
}

struct pco_fadd_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool sat;
};

#define pco_fadd_(func, dest0, src0, src1, ...) _pco_fadd_(func, dest0, src0, src1, (struct pco_fadd_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fadd_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_fadd_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_FADD;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_sat(instr, mods.sat);

   return instr;
}

#define pco_fadd(b, dest0, src0, src1, ...) _pco_fadd(b, dest0, src0, src1, (struct pco_fadd_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fadd(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_fadd_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fadd_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fmul_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool sat;
};

#define pco_fmul_(func, dest0, src0, src1, ...) _pco_fmul_(func, dest0, src0, src1, (struct pco_fmul_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fmul_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_fmul_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_FMUL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_sat(instr, mods.sat);

   return instr;
}

#define pco_fmul(b, dest0, src0, src1, ...) _pco_fmul(b, dest0, src0, src1, (struct pco_fmul_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fmul(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_fmul_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fmul_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fmad_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool sat;
  bool lp;
};

#define pco_fmad_(func, dest0, src0, src1, src2, ...) _pco_fmad_(func, dest0, src0, src1, src2, (struct pco_fmad_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fmad_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fmad_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_FMAD;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_sat(instr, mods.sat);
   pco_instr_set_lp(instr, mods.lp);

   return instr;
}

#define pco_fmad(b, dest0, src0, src1, src2, ...) _pco_fmad(b, dest0, src0, src1, src2, (struct pco_fmad_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fmad(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fmad_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fmad_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_frcp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_frcp_(func, dest0, src0, ...) _pco_frcp_(func, dest0, src0, (struct pco_frcp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_frcp_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_frcp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FRCP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_frcp(b, dest0, src0, ...) _pco_frcp(b, dest0, src0, (struct pco_frcp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_frcp(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_frcp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_frcp_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_frsq_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_frsq_(func, dest0, src0, ...) _pco_frsq_(func, dest0, src0, (struct pco_frsq_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_frsq_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_frsq_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FRSQ;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_frsq(b, dest0, src0, ...) _pco_frsq(b, dest0, src0, (struct pco_frsq_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_frsq(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_frsq_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_frsq_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_flog_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_flog_(func, dest0, src0, ...) _pco_flog_(func, dest0, src0, (struct pco_flog_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_flog_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_flog_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FLOG;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_flog(b, dest0, src0, ...) _pco_flog(b, dest0, src0, (struct pco_flog_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_flog(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_flog_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_flog_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_flogcn_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_flogcn_(func, dest0, src0, ...) _pco_flogcn_(func, dest0, src0, (struct pco_flogcn_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_flogcn_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_flogcn_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FLOGCN;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_flogcn(b, dest0, src0, ...) _pco_flogcn(b, dest0, src0, (struct pco_flogcn_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_flogcn(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_flogcn_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_flogcn_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fexp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fexp_(func, dest0, src0, ...) _pco_fexp_(func, dest0, src0, (struct pco_fexp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fexp_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fexp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FEXP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fexp(b, dest0, src0, ...) _pco_fexp(b, dest0, src0, (struct pco_fexp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fexp(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fexp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fexp_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fred_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_fred_type fred_type;
  enum pco_fred_part fred_part;
};

#define pco_fred_(func, dest0, dest1, dest2, src0, src1, src2, ...) _pco_fred_(func, dest0, dest1, dest2, src0, src1, src2, (struct pco_fred_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fred_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fred_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       3,
                                       3);

   instr->op = PCO_OP_FRED;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->dest[2] = dest2;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_fred_type(instr, mods.fred_type);
   pco_instr_set_fred_part(instr, mods.fred_part);

   return instr;
}

#define pco_fred(b, dest0, dest1, dest2, src0, src1, src2, ...) _pco_fred(b, dest0, dest1, dest2, src0, src1, src2, (struct pco_fred_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fred(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fred_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fred_(func, dest0, dest1, dest2, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fsinc_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fsinc_(func, dest0, dest1, src0, ...) _pco_fsinc_(func, dest0, dest1, src0, (struct pco_fsinc_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fsinc_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, struct pco_fsinc_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       1);

   instr->op = PCO_OP_FSINC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fsinc(b, dest0, dest1, src0, ...) _pco_fsinc(b, dest0, dest1, src0, (struct pco_fsinc_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fsinc(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, struct pco_fsinc_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fsinc_(func, dest0, dest1, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_mbyp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_mbyp_(func, dest0, src0, ...) _pco_mbyp_(func, dest0, src0, (struct pco_mbyp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_mbyp_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_mbyp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_MBYP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_mbyp(b, dest0, src0, ...) _pco_mbyp(b, dest0, src0, (struct pco_mbyp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_mbyp(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_mbyp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_mbyp_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fdsx_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fdsx_(func, dest0, src0, ...) _pco_fdsx_(func, dest0, src0, (struct pco_fdsx_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fdsx_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fdsx_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FDSX;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fdsx(b, dest0, src0, ...) _pco_fdsx(b, dest0, src0, (struct pco_fdsx_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fdsx(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fdsx_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fdsx_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fdsxf_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fdsxf_(func, dest0, src0, ...) _pco_fdsxf_(func, dest0, src0, (struct pco_fdsxf_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fdsxf_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fdsxf_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FDSXF;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fdsxf(b, dest0, src0, ...) _pco_fdsxf(b, dest0, src0, (struct pco_fdsxf_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fdsxf(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fdsxf_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fdsxf_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fdsy_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fdsy_(func, dest0, src0, ...) _pco_fdsy_(func, dest0, src0, (struct pco_fdsy_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fdsy_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fdsy_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FDSY;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fdsy(b, dest0, src0, ...) _pco_fdsy(b, dest0, src0, (struct pco_fdsy_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fdsy(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fdsy_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fdsy_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fdsyf_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fdsyf_(func, dest0, src0, ...) _pco_fdsyf_(func, dest0, src0, (struct pco_fdsyf_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fdsyf_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fdsyf_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FDSYF;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fdsyf(b, dest0, src0, ...) _pco_fdsyf(b, dest0, src0, (struct pco_fdsyf_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fdsyf(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fdsyf_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fdsyf_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_pck_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_pck_fmt pck_fmt;
  bool roundzero;
  bool scale;
};

#define pco_pck_(func, dest0, src0, ...) _pco_pck_(func, dest0, src0, (struct pco_pck_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_pck_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_pck_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_PCK;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_pck_fmt(instr, mods.pck_fmt);
   pco_instr_set_roundzero(instr, mods.roundzero);
   pco_instr_set_scale(instr, mods.scale);

   return instr;
}

#define pco_pck(b, dest0, src0, ...) _pco_pck(b, dest0, src0, (struct pco_pck_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_pck(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_pck_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_pck_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_pck_prog_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool roundzero;
  bool scale;
};

#define pco_pck_prog_(func, dest0, src0, src1, ...) _pco_pck_prog_(func, dest0, src0, src1, (struct pco_pck_prog_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_pck_prog_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_pck_prog_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_PCK_PROG;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_roundzero(instr, mods.roundzero);
   pco_instr_set_scale(instr, mods.scale);

   return instr;
}

#define pco_pck_prog(b, dest0, src0, src1, ...) _pco_pck_prog(b, dest0, src0, src1, (struct pco_pck_prog_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_pck_prog(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_pck_prog_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_pck_prog_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_add64_32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool s;
};

#define pco_add64_32_(func, dest0, dest1, src0, src1, src2, src3, ...) _pco_add64_32_(func, dest0, dest1, src0, src1, src2, src3, (struct pco_add64_32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_add64_32_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_add64_32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       4);

   instr->op = PCO_OP_ADD64_32;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_add64_32(b, dest0, dest1, src0, src1, src2, src3, ...) _pco_add64_32(b, dest0, dest1, src0, src1, src2, src3, (struct pco_add64_32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_add64_32(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_add64_32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_add64_32_(func, dest0, dest1, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_imadd64_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool s;
};

#define pco_imadd64_(func, dest0, dest1, src0, src1, src2, src3, src4, ...) _pco_imadd64_(func, dest0, dest1, src0, src1, src2, src3, src4, (struct pco_imadd64_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_imadd64_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_imadd64_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       5);

   instr->op = PCO_OP_IMADD64;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_imadd64(b, dest0, dest1, src0, src1, src2, src3, src4, ...) _pco_imadd64(b, dest0, dest1, src0, src1, src2, src3, src4, (struct pco_imadd64_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_imadd64(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_imadd64_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_imadd64_(func, dest0, dest1, src0, src1, src2, src3, src4, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_imadd32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool s;
};

#define pco_imadd32_(func, dest0, src0, src1, src2, src3, ...) _pco_imadd32_(func, dest0, src0, src1, src2, src3, (struct pco_imadd32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_imadd32_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_imadd32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_IMADD32;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_imadd32(b, dest0, src0, src1, src2, src3, ...) _pco_imadd32(b, dest0, src0, src1, src2, src3, (struct pco_imadd32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_imadd32(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_imadd32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_imadd32_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_unpck_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_pck_fmt pck_fmt;
  bool roundzero;
  bool scale;
};

#define pco_unpck_(func, dest0, src0, ...) _pco_unpck_(func, dest0, src0, (struct pco_unpck_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_unpck_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_unpck_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_UNPCK;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_pck_fmt(instr, mods.pck_fmt);
   pco_instr_set_roundzero(instr, mods.roundzero);
   pco_instr_set_scale(instr, mods.scale);

   return instr;
}

#define pco_unpck(b, dest0, src0, ...) _pco_unpck(b, dest0, src0, (struct pco_unpck_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_unpck(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_unpck_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_unpck_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_tst_mods {
  enum pco_tst_op_main tst_op_main;
  bool phase2end;
  enum pco_tst_type_main tst_type_main;
};

#define pco_tst_(func, dest0, dest1, src0, src1, ...) _pco_tst_(func, dest0, dest1, src0, src1, (struct pco_tst_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_tst_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_tst_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_TST;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_tst_op_main(instr, mods.tst_op_main);
   pco_instr_set_phase2end(instr, mods.phase2end);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

struct pco_movc_mods {
  bool phase2end;
};

#define pco_movc_(func, dest0, dest1, src0, src1, src2, src3, src4, ...) _pco_movc_(func, dest0, dest1, src0, src1, src2, src3, src4, (struct pco_movc_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_movc_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_movc_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       5);

   instr->op = PCO_OP_MOVC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;

   pco_instr_set_phase2end(instr, mods.phase2end);

   return instr;
}

struct pco_movwm_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool phase2end;
};

#define pco_movwm_(func, dest0, src0, src1, ...) _pco_movwm_(func, dest0, src0, src1, (struct pco_movwm_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_movwm_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_movwm_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_MOVWM;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_phase2end(instr, mods.phase2end);

   return instr;
}

#define pco_movwm(b, dest0, src0, src1, ...) _pco_movwm(b, dest0, src0, src1, (struct pco_movwm_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_movwm(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_movwm_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_movwm_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_movs1_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_movs1_(func, dest0, src0, ...) _pco_movs1_(func, dest0, src0, (struct pco_movs1_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_movs1_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_movs1_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_MOVS1;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_movs1(b, dest0, src0, ...) _pco_movs1(b, dest0, src0, (struct pco_movs1_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_movs1(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_movs1_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_movs1_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uvsw_write_mods {
  enum pco_exec_cnd exec_cnd;
  unsigned rpt;
};

#define pco_uvsw_write_(func, src0, src1, ...) _pco_uvsw_write_(func, src0, src1, (struct pco_uvsw_write_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uvsw_write_(pco_func *func, pco_ref src0, pco_ref src1, struct pco_uvsw_write_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       2);

   instr->op = PCO_OP_UVSW_WRITE;

   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_uvsw_write(b, src0, src1, ...) _pco_uvsw_write(b, src0, src1, (struct pco_uvsw_write_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uvsw_write(pco_builder *b, pco_ref src0, pco_ref src1, struct pco_uvsw_write_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uvsw_write_(func, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uvsw_emit_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_uvsw_emit_(func, ...) _pco_uvsw_emit_(func, (struct pco_uvsw_emit_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uvsw_emit_(pco_func *func, struct pco_uvsw_emit_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_UVSW_EMIT;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_uvsw_emit(b, ...) _pco_uvsw_emit(b, (struct pco_uvsw_emit_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uvsw_emit(pco_builder *b, struct pco_uvsw_emit_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uvsw_emit_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uvsw_endtask_mods {
  bool end;
};

#define pco_uvsw_endtask_(func, ...) _pco_uvsw_endtask_(func, (struct pco_uvsw_endtask_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uvsw_endtask_(pco_func *func, struct pco_uvsw_endtask_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_UVSW_ENDTASK;


   pco_instr_set_end(instr, mods.end);

   return instr;
}

#define pco_uvsw_endtask(b, ...) _pco_uvsw_endtask(b, (struct pco_uvsw_endtask_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uvsw_endtask(pco_builder *b, struct pco_uvsw_endtask_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uvsw_endtask_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uvsw_emit_endtask_mods {
  bool end;
};

#define pco_uvsw_emit_endtask_(func, ...) _pco_uvsw_emit_endtask_(func, (struct pco_uvsw_emit_endtask_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uvsw_emit_endtask_(pco_func *func, struct pco_uvsw_emit_endtask_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_UVSW_EMIT_ENDTASK;


   pco_instr_set_end(instr, mods.end);

   return instr;
}

#define pco_uvsw_emit_endtask(b, ...) _pco_uvsw_emit_endtask(b, (struct pco_uvsw_emit_endtask_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uvsw_emit_endtask(pco_builder *b, struct pco_uvsw_emit_endtask_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uvsw_emit_endtask_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uvsw_write_emit_endtask_mods {
  bool end;
};

#define pco_uvsw_write_emit_endtask_(func, src0, src1, ...) _pco_uvsw_write_emit_endtask_(func, src0, src1, (struct pco_uvsw_write_emit_endtask_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uvsw_write_emit_endtask_(pco_func *func, pco_ref src0, pco_ref src1, struct pco_uvsw_write_emit_endtask_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       2);

   instr->op = PCO_OP_UVSW_WRITE_EMIT_ENDTASK;

   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_end(instr, mods.end);

   return instr;
}

#define pco_uvsw_write_emit_endtask(b, src0, src1, ...) _pco_uvsw_write_emit_endtask(b, src0, src1, (struct pco_uvsw_write_emit_endtask_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uvsw_write_emit_endtask(pco_builder *b, pco_ref src0, pco_ref src1, struct pco_uvsw_write_emit_endtask_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uvsw_write_emit_endtask_(func, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fitr_mods {
  enum pco_exec_cnd exec_cnd;
  bool end;
  enum pco_itr_mode itr_mode;
  bool sat;
};

#define pco_fitr_(func, dest0, src0, src1, src2, ...) _pco_fitr_(func, dest0, src0, src1, src2, (struct pco_fitr_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fitr_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fitr_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_FITR;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);

   return instr;
}

#define pco_fitr(b, dest0, src0, src1, src2, ...) _pco_fitr(b, dest0, src0, src1, src2, (struct pco_fitr_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fitr(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_fitr_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fitr_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fitrp_mods {
  enum pco_exec_cnd exec_cnd;
  bool end;
  enum pco_itr_mode itr_mode;
  bool sat;
};

#define pco_fitrp_(func, dest0, src0, src1, src2, src3, ...) _pco_fitrp_(func, dest0, src0, src1, src2, src3, (struct pco_fitrp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fitrp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_fitrp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_FITRP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);

   return instr;
}

#define pco_fitrp(b, dest0, src0, src1, src2, src3, ...) _pco_fitrp(b, dest0, src0, src1, src2, src3, (struct pco_fitrp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fitrp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_fitrp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fitrp_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ld_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_mcu_cache_mode_ld mcu_cache_mode_ld;
};

#define pco_ld_(func, dest0, src0, src1, src2, ...) _pco_ld_(func, dest0, src0, src1, src2, (struct pco_ld_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ld_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ld_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_LD;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_mcu_cache_mode_ld(instr, mods.mcu_cache_mode_ld);

   return instr;
}

#define pco_ld(b, dest0, src0, src1, src2, ...) _pco_ld(b, dest0, src0, src1, src2, (struct pco_ld_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ld(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ld_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ld_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ld_regbl_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_mcu_cache_mode_ld mcu_cache_mode_ld;
};

#define pco_ld_regbl_(func, dest0, src0, src1, src2, ...) _pco_ld_regbl_(func, dest0, src0, src1, src2, (struct pco_ld_regbl_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ld_regbl_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ld_regbl_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_LD_REGBL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_mcu_cache_mode_ld(instr, mods.mcu_cache_mode_ld);

   return instr;
}

#define pco_ld_regbl(b, dest0, src0, src1, src2, ...) _pco_ld_regbl(b, dest0, src0, src1, src2, (struct pco_ld_regbl_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ld_regbl(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ld_regbl_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ld_regbl_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_st_mods {
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool idf;
};

#define pco_st_(func, src0, src1, src2, src3, src4, src5, ...) _pco_st_(func, src0, src1, src2, src3, src4, src5, (struct pco_st_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_st_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_st_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       6);

   instr->op = PCO_OP_ST;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;

   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_idf(instr, mods.idf);

   return instr;
}

struct pco_st_regbl_mods {
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool idf;
};

#define pco_st_regbl_(func, src0, src1, src2, src3, src4, src5, ...) _pco_st_regbl_(func, src0, src1, src2, src3, src4, src5, (struct pco_st_regbl_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_st_regbl_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_st_regbl_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       6);

   instr->op = PCO_OP_ST_REGBL;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;

   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_idf(instr, mods.idf);

   return instr;
}

struct pco_st_tiled_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
};

#define pco_st_tiled_(func, src0, src1, src2, src3, src4, src5, ...) _pco_st_tiled_(func, src0, src1, src2, src3, src4, src5, (struct pco_st_tiled_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_st_tiled_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_st_tiled_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       6);

   instr->op = PCO_OP_ST_TILED;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);

   return instr;
}

#define pco_st_tiled(b, src0, src1, src2, src3, src4, src5, ...) _pco_st_tiled(b, src0, src1, src2, src3, src4, src5, (struct pco_st_tiled_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_st_tiled(pco_builder *b, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_st_tiled_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_st_tiled_(func, src0, src1, src2, src3, src4, src5, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_idf_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_idf_(func, src0, src1, ...) _pco_idf_(func, src0, src1, (struct pco_idf_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_idf_(pco_func *func, pco_ref src0, pco_ref src1, struct pco_idf_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       2);

   instr->op = PCO_OP_IDF;

   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_idf(b, src0, src1, ...) _pco_idf(b, src0, src1, (struct pco_idf_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_idf(pco_builder *b, pco_ref src0, pco_ref src1, struct pco_idf_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_idf_(func, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  enum pco_atom_op atom_op;
};

#define pco_atomic_(func, dest0, src0, src1, ...) _pco_atomic_(func, dest0, src0, src1, (struct pco_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_atomic_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_ATOMIC;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom_op(instr, mods.atom_op);

   return instr;
}

#define pco_atomic(b, dest0, src0, src1, ...) _pco_atomic(b, dest0, src0, src1, (struct pco_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_atomic(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_atomic_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_smp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_dim dim;
  bool proj;
  bool fcnorm;
  bool nncoords;
  enum pco_lod_mode lod_mode;
  bool pplod;
  bool tao;
  bool soo;
  bool sno;
  enum pco_sb_mode sb_mode;
  enum pco_mcu_cache_mode_ld mcu_cache_mode_ld;
  bool array;
  bool integer;
  enum pco_schedswap schedswap;
  bool f16;
};

#define pco_smp_(func, dest0, src0, src1, src2, src3, src4, src5, ...) _pco_smp_(func, dest0, src0, src1, src2, src3, src4, src5, (struct pco_smp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_smp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_smp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       6);

   instr->op = PCO_OP_SMP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_dim(instr, mods.dim);
   pco_instr_set_proj(instr, mods.proj);
   pco_instr_set_fcnorm(instr, mods.fcnorm);
   pco_instr_set_nncoords(instr, mods.nncoords);
   pco_instr_set_lod_mode(instr, mods.lod_mode);
   pco_instr_set_pplod(instr, mods.pplod);
   pco_instr_set_tao(instr, mods.tao);
   pco_instr_set_soo(instr, mods.soo);
   pco_instr_set_sno(instr, mods.sno);
   pco_instr_set_sb_mode(instr, mods.sb_mode);
   pco_instr_set_mcu_cache_mode_ld(instr, mods.mcu_cache_mode_ld);
   pco_instr_set_array(instr, mods.array);
   pco_instr_set_integer(instr, mods.integer);
   pco_instr_set_schedswap(instr, mods.schedswap);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_smp(b, dest0, src0, src1, src2, src3, src4, src5, ...) _pco_smp(b, dest0, src0, src1, src2, src3, src4, src5, (struct pco_smp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_smp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_smp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_smp_(func, dest0, src0, src1, src2, src3, src4, src5, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_smp_wrt_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_dim dim;
  bool proj;
  bool fcnorm;
  bool nncoords;
  enum pco_lod_mode lod_mode;
  bool pplod;
  bool tao;
  bool soo;
  bool sno;
  enum pco_sb_mode sb_mode;
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool array;
  bool integer;
  enum pco_schedswap schedswap;
  bool f16;
};

#define pco_smp_wrt_(func, src0, src1, src2, src3, src4, src5, ...) _pco_smp_wrt_(func, src0, src1, src2, src3, src4, src5, (struct pco_smp_wrt_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_smp_wrt_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_smp_wrt_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       6);

   instr->op = PCO_OP_SMP_WRT;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_dim(instr, mods.dim);
   pco_instr_set_proj(instr, mods.proj);
   pco_instr_set_fcnorm(instr, mods.fcnorm);
   pco_instr_set_nncoords(instr, mods.nncoords);
   pco_instr_set_lod_mode(instr, mods.lod_mode);
   pco_instr_set_pplod(instr, mods.pplod);
   pco_instr_set_tao(instr, mods.tao);
   pco_instr_set_soo(instr, mods.soo);
   pco_instr_set_sno(instr, mods.sno);
   pco_instr_set_sb_mode(instr, mods.sb_mode);
   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_array(instr, mods.array);
   pco_instr_set_integer(instr, mods.integer);
   pco_instr_set_schedswap(instr, mods.schedswap);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_smp_wrt(b, src0, src1, src2, src3, src4, src5, ...) _pco_smp_wrt(b, src0, src1, src2, src3, src4, src5, (struct pco_smp_wrt_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_smp_wrt(pco_builder *b, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, struct pco_smp_wrt_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_smp_wrt_(func, src0, src1, src2, src3, src4, src5, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_alphatst_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_alphatst_(func, dest0, src0, src1, src2, src3, ...) _pco_alphatst_(func, dest0, src0, src1, src2, src3, (struct pco_alphatst_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_alphatst_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_alphatst_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_ALPHATST;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_alphatst(b, dest0, src0, src1, src2, src3, ...) _pco_alphatst(b, dest0, src0, src1, src2, src3, (struct pco_alphatst_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_alphatst(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_alphatst_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_alphatst_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_alphaf_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_alphaf_(func, dest0, src0, src1, src2, src3, ...) _pco_alphaf_(func, dest0, src0, src1, src2, src3, (struct pco_alphaf_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_alphaf_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_alphaf_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_ALPHAF;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_alphaf(b, dest0, src0, src1, src2, src3, ...) _pco_alphaf(b, dest0, src0, src1, src2, src3, (struct pco_alphaf_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_alphaf(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_alphaf_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_alphaf_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_depthf_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_depthf_(func, src0, src1, ...) _pco_depthf_(func, src0, src1, (struct pco_depthf_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_depthf_(pco_func *func, pco_ref src0, pco_ref src1, struct pco_depthf_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       2);

   instr->op = PCO_OP_DEPTHF;

   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_depthf(b, src0, src1, ...) _pco_depthf(b, src0, src1, (struct pco_depthf_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_depthf(pco_builder *b, pco_ref src0, pco_ref src1, struct pco_depthf_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_depthf_(func, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_savmsk_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_savmsk_mode savmsk_mode;
};

#define pco_savmsk_(func, dest0, dest1, ...) _pco_savmsk_(func, dest0, dest1, (struct pco_savmsk_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_savmsk_(pco_func *func, pco_ref dest0, pco_ref dest1, struct pco_savmsk_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       0);

   instr->op = PCO_OP_SAVMSK;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_savmsk_mode(instr, mods.savmsk_mode);

   return instr;
}

#define pco_savmsk(b, dest0, dest1, ...) _pco_savmsk(b, dest0, dest1, (struct pco_savmsk_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_savmsk(pco_builder *b, pco_ref dest0, pco_ref dest1, struct pco_savmsk_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_savmsk_(func, dest0, dest1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_emitpix_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  bool freep;
};

#define pco_emitpix_(func, src0, src1, ...) _pco_emitpix_(func, src0, src1, (struct pco_emitpix_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_emitpix_(pco_func *func, pco_ref src0, pco_ref src1, struct pco_emitpix_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       2);

   instr->op = PCO_OP_EMITPIX;

   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_freep(instr, mods.freep);

   return instr;
}

#define pco_emitpix(b, src0, src1, ...) _pco_emitpix(b, src0, src1, (struct pco_emitpix_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_emitpix(pco_builder *b, pco_ref src0, pco_ref src1, struct pco_emitpix_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_emitpix_(func, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_setl_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_setl_(func, src0, ...) _pco_setl_(func, src0, (struct pco_setl_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_setl_(pco_func *func, pco_ref src0, struct pco_setl_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       1);

   instr->op = PCO_OP_SETL;

   instr->src[0] = src0;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_setl(b, src0, ...) _pco_setl(b, src0, (struct pco_setl_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_setl(pco_builder *b, pco_ref src0, struct pco_setl_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_setl_(func, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_movi32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_movi32_(func, dest0, src0, ...) _pco_movi32_(func, dest0, src0, (struct pco_movi32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_movi32_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_movi32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_MOVI32;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_movi32(b, dest0, src0, ...) _pco_movi32(b, dest0, src0, (struct pco_movi32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_movi32(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_movi32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_movi32_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cbs_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_cbs_(func, dest0, src0, ...) _pco_cbs_(func, dest0, src0, (struct pco_cbs_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cbs_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_cbs_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_CBS;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_cbs(b, dest0, src0, ...) _pco_cbs(b, dest0, src0, (struct pco_cbs_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cbs(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_cbs_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cbs_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ftb_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_ftb_(func, dest0, src0, ...) _pco_ftb_(func, dest0, src0, (struct pco_ftb_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ftb_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_ftb_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FTB;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_ftb(b, dest0, src0, ...) _pco_ftb(b, dest0, src0, (struct pco_ftb_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ftb(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_ftb_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ftb_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_rev_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_rev_(func, dest0, src0, ...) _pco_rev_(func, dest0, src0, (struct pco_rev_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_rev_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_rev_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_REV;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_rev(b, dest0, src0, ...) _pco_rev(b, dest0, src0, (struct pco_rev_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_rev(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_rev_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_rev_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_shuffle_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_shuffle_(func, dest0, src0, src1, ...) _pco_shuffle_(func, dest0, src0, src1, (struct pco_shuffle_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_shuffle_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_shuffle_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_SHUFFLE;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_shuffle(b, dest0, src0, src1, ...) _pco_shuffle(b, dest0, src0, src1, (struct pco_shuffle_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_shuffle(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_shuffle_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_shuffle_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_logical_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_logiop logiop;
};

#define pco_logical_(func, dest0, src0, src1, src2, src3, ...) _pco_logical_(func, dest0, src0, src1, src2, src3, (struct pco_logical_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_logical_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_logical_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_LOGICAL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_logiop(instr, mods.logiop);

   return instr;
}

#define pco_logical(b, dest0, src0, src1, src2, src3, ...) _pco_logical(b, dest0, src0, src1, src2, src3, (struct pco_logical_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_logical(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_logical_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_logical_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_shift_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_shiftop shiftop;
};

#define pco_shift_(func, dest0, src0, src1, src2, ...) _pco_shift_(func, dest0, src0, src1, src2, (struct pco_shift_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_shift_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_shift_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_SHIFT;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_shiftop(instr, mods.shiftop);

   return instr;
}

#define pco_shift(b, dest0, src0, src1, src2, ...) _pco_shift(b, dest0, src0, src1, src2, (struct pco_shift_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_shift(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_shift_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_shift_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_copysign_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_copysign_(func, dest0, src0, src1, ...) _pco_copysign_(func, dest0, src0, src1, (struct pco_copysign_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_copysign_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_copysign_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_COPYSIGN;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_copysign(b, dest0, src0, src1, ...) _pco_copysign(b, dest0, src0, src1, (struct pco_copysign_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_copysign(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_copysign_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_copysign_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ibfe_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_ibfe_(func, dest0, src0, src1, src2, ...) _pco_ibfe_(func, dest0, src0, src1, src2, (struct pco_ibfe_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ibfe_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ibfe_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_IBFE;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_ibfe(b, dest0, src0, src1, src2, ...) _pco_ibfe(b, dest0, src0, src1, src2, (struct pco_ibfe_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ibfe(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ibfe_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ibfe_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ubfe_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_ubfe_(func, dest0, src0, src1, src2, ...) _pco_ubfe_(func, dest0, src0, src1, src2, (struct pco_ubfe_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ubfe_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ubfe_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_UBFE;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_ubfe(b, dest0, src0, src1, src2, ...) _pco_ubfe(b, dest0, src0, src1, src2, (struct pco_ubfe_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ubfe(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ubfe_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ubfe_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_bfi_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_bfi_(func, dest0, src0, src1, src2, src3, ...) _pco_bfi_(func, dest0, src0, src1, src2, src3, (struct pco_bfi_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_bfi_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_bfi_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_BFI;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_bfi(b, dest0, src0, src1, src2, src3, ...) _pco_bfi(b, dest0, src0, src1, src2, src3, (struct pco_bfi_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_bfi(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_bfi_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_bfi_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#define pco_bbyp0bm_(func, dest0, dest1, src0, src1) _pco_bbyp0bm_(func, dest0, dest1, src0, src1)
static
pco_instr *_pco_bbyp0bm_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_BBYP0BM;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;


   return instr;
}

#define pco_bbyp0bm_imm32_(func, dest0, dest1, src0, src1) _pco_bbyp0bm_imm32_(func, dest0, dest1, src0, src1)
static
pco_instr *_pco_bbyp0bm_imm32_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_BBYP0BM_IMM32;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;


   return instr;
}

#define pco_bbyp0s1_(func, dest0, dest1, src0) _pco_bbyp0s1_(func, dest0, dest1, src0)
static
pco_instr *_pco_bbyp0s1_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       1);

   instr->op = PCO_OP_BBYP0S1;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;


   return instr;
}

#define pco_msk_bbyp0s1_(func, dest0, dest1, dest2, src0, src1, src2) _pco_msk_bbyp0s1_(func, dest0, dest1, dest2, src0, src1, src2)
static
pco_instr *_pco_msk_bbyp0s1_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, pco_ref src2)
{
   pco_instr *instr = pco_instr_create(func,
                                       3,
                                       3);

   instr->op = PCO_OP_MSK_BBYP0S1;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->dest[2] = dest2;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;


   return instr;
}

#define pco_msk_lsl_(func, dest0, dest1, dest2, src0, src1, src2) _pco_msk_lsl_(func, dest0, dest1, dest2, src0, src1, src2)
static
pco_instr *_pco_msk_lsl_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, pco_ref src2)
{
   pco_instr *instr = pco_instr_create(func,
                                       3,
                                       3);

   instr->op = PCO_OP_MSK_LSL;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->dest[2] = dest2;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;


   return instr;
}

#define pco_wop_(func) _pco_wop_(func)
static
pco_instr *_pco_wop_(pco_func *func)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_WOP;



   return instr;
}

#define pco_wop(b) _pco_wop(b)
static inline
pco_instr *_pco_wop(pco_builder *b)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_wop_(func);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#define pco_wdf_(func, src0) _pco_wdf_(func, src0)
static
pco_instr *_pco_wdf_(pco_func *func, pco_ref src0)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       1);

   instr->op = PCO_OP_WDF;

   instr->src[0] = src0;


   return instr;
}

#define pco_wdf(b, src0) _pco_wdf(b, src0)
static inline
pco_instr *_pco_wdf(pco_builder *b, pco_ref src0)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_wdf_(func, src0);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_nop_mods {
  enum pco_exec_cnd exec_cnd;
  bool end;
};

#define pco_nop_(func, ...) _pco_nop_(func, (struct pco_nop_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_nop_(pco_func *func, struct pco_nop_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_NOP;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);

   return instr;
}

#define pco_nop(b, ...) _pco_nop(b, (struct pco_nop_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_nop(pco_builder *b, struct pco_nop_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_nop_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ditr_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_itr_mode itr_mode;
  bool sat;
  enum pco_sched sched;
  bool f16;
};

#define pco_ditr_(func, dest0, src0, src1, src2, ...) _pco_ditr_(func, dest0, src0, src1, src2, (struct pco_ditr_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ditr_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ditr_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_DITR;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);
   pco_instr_set_sched(instr, mods.sched);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_ditr(b, dest0, src0, src1, src2, ...) _pco_ditr(b, dest0, src0, src1, src2, (struct pco_ditr_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ditr(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ditr_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ditr_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ditrp_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_itr_mode itr_mode;
  bool sat;
  enum pco_sched sched;
  bool f16;
};

#define pco_ditrp_(func, dest0, src0, src1, src2, src3, ...) _pco_ditrp_(func, dest0, src0, src1, src2, src3, (struct pco_ditrp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ditrp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_ditrp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_DITRP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);
   pco_instr_set_sched(instr, mods.sched);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_ditrp(b, dest0, src0, src1, src2, src3, ...) _pco_ditrp(b, dest0, src0, src1, src2, src3, (struct pco_ditrp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ditrp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_ditrp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ditrp_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ditrp_write_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_itr_mode itr_mode;
  bool sat;
  enum pco_sched sched;
  bool f16;
};

#define pco_ditrp_write_(func, dest0, src0, src1, src2, src3, ...) _pco_ditrp_write_(func, dest0, src0, src1, src2, src3, (struct pco_ditrp_write_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ditrp_write_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_ditrp_write_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_DITRP_WRITE;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);
   pco_instr_set_sched(instr, mods.sched);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_ditrp_write(b, dest0, src0, src1, src2, src3, ...) _pco_ditrp_write(b, dest0, src0, src1, src2, src3, (struct pco_ditrp_write_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ditrp_write(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_ditrp_write_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ditrp_write_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_ditrp_read_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_itr_mode itr_mode;
  bool sat;
  enum pco_sched sched;
  bool f16;
};

#define pco_ditrp_read_(func, dest0, src0, src1, src2, ...) _pco_ditrp_read_(func, dest0, src0, src1, src2, (struct pco_ditrp_read_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_ditrp_read_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ditrp_read_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_DITRP_READ;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_itr_mode(instr, mods.itr_mode);
   pco_instr_set_sat(instr, mods.sat);
   pco_instr_set_sched(instr, mods.sched);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_ditrp_read(b, dest0, src0, src1, src2, ...) _pco_ditrp_read(b, dest0, src0, src1, src2, (struct pco_ditrp_read_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_ditrp_read(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_ditrp_read_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_ditrp_read_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cndst_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_cnd cnd;
};

#define pco_cndst_(func, dest0, dest1, src0, src1, ...) _pco_cndst_(func, dest0, dest1, src0, src1, (struct pco_cndst_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cndst_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndst_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_CNDST;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_cnd(instr, mods.cnd);

   return instr;
}

#define pco_cndst(b, dest0, dest1, src0, src1, ...) _pco_cndst(b, dest0, dest1, src0, src1, (struct pco_cndst_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cndst(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndst_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cndst_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cndef_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_cnd cnd;
};

#define pco_cndef_(func, dest0, dest1, src0, src1, ...) _pco_cndef_(func, dest0, dest1, src0, src1, (struct pco_cndef_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cndef_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndef_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_CNDEF;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_cnd(instr, mods.cnd);

   return instr;
}

#define pco_cndef(b, dest0, dest1, src0, src1, ...) _pco_cndef(b, dest0, dest1, src0, src1, (struct pco_cndef_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cndef(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndef_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cndef_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cndsm_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_cnd cnd;
};

#define pco_cndsm_(func, dest0, dest1, src0, src1, ...) _pco_cndsm_(func, dest0, dest1, src0, src1, (struct pco_cndsm_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cndsm_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndsm_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_CNDSM;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_cnd(instr, mods.cnd);

   return instr;
}

#define pco_cndsm(b, dest0, dest1, src0, src1, ...) _pco_cndsm(b, dest0, dest1, src0, src1, (struct pco_cndsm_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cndsm(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndsm_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cndsm_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cndlt_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_cnd cnd;
};

#define pco_cndlt_(func, dest0, dest1, dest2, src0, src1, ...) _pco_cndlt_(func, dest0, dest1, dest2, src0, src1, (struct pco_cndlt_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cndlt_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, struct pco_cndlt_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       3,
                                       2);

   instr->op = PCO_OP_CNDLT;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->dest[2] = dest2;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_cnd(instr, mods.cnd);

   return instr;
}

#define pco_cndlt(b, dest0, dest1, dest2, src0, src1, ...) _pco_cndlt(b, dest0, dest1, dest2, src0, src1, (struct pco_cndlt_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cndlt(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref dest2, pco_ref src0, pco_ref src1, struct pco_cndlt_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cndlt_(func, dest0, dest1, dest2, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cndend_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_cndend_(func, dest0, dest1, src0, src1, ...) _pco_cndend_(func, dest0, dest1, src0, src1, (struct pco_cndend_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cndend_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndend_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_CNDEND;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_cndend(b, dest0, dest1, src0, src1, ...) _pco_cndend(b, dest0, dest1, src0, src1, (struct pco_cndend_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cndend(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_cndend_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cndend_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_br_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_branch_cnd branch_cnd;
  bool link;
};

#define pco_br_(func, target_cf_node, ...) _pco_br_(func, target_cf_node, (struct pco_br_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_br_(pco_func *func, pco_cf_node *target_cf_node, struct pco_br_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_BR;

   instr->target_cf_node = target_cf_node;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_branch_cnd(instr, mods.branch_cnd);
   pco_instr_set_link(instr, mods.link);

   return instr;
}

#define pco_br(b, target_cf_node, ...) _pco_br(b, target_cf_node, (struct pco_br_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_br(pco_builder *b, pco_cf_node *target_cf_node, struct pco_br_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_br_(func, target_cf_node, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_br_next_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_br_next_(func, ...) _pco_br_next_(func, (struct pco_br_next_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_br_next_(pco_func *func, struct pco_br_next_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_BR_NEXT;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_br_next(b, ...) _pco_br_next(b, (struct pco_br_next_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_br_next(pco_builder *b, struct pco_br_next_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_br_next_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_br_skip_next_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_branch_cnd branch_cnd;
};

#define pco_br_skip_next_(func, ...) _pco_br_skip_next_(func, (struct pco_br_skip_next_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_br_skip_next_(pco_func *func, struct pco_br_skip_next_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_BR_SKIP_NEXT;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_branch_cnd(instr, mods.branch_cnd);

   return instr;
}

#define pco_br_skip_next(b, ...) _pco_br_skip_next(b, (struct pco_br_skip_next_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_br_skip_next(pco_builder *b, struct pco_br_skip_next_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_br_skip_next_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_mutex_mods {
  enum pco_mutex_op mutex_op;
};

#define pco_mutex_(func, src0, ...) _pco_mutex_(func, src0, (struct pco_mutex_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_mutex_(pco_func *func, pco_ref src0, struct pco_mutex_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       1);

   instr->op = PCO_OP_MUTEX;

   instr->src[0] = src0;

   pco_instr_set_mutex_op(instr, mods.mutex_op);

   return instr;
}

#define pco_mutex(b, src0, ...) _pco_mutex(b, src0, (struct pco_mutex_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_mutex(pco_builder *b, pco_ref src0, struct pco_mutex_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_mutex_(func, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_savl_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_savl_(func, dest0, ...) _pco_savl_(func, dest0, (struct pco_savl_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_savl_(pco_func *func, pco_ref dest0, struct pco_savl_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       0);

   instr->op = PCO_OP_SAVL;

   instr->dest[0] = dest0;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_savl(b, dest0, ...) _pco_savl(b, dest0, (struct pco_savl_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_savl(pco_builder *b, pco_ref dest0, struct pco_savl_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_savl_(func, dest0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_scmp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_op_main tst_op_main;
};

#define pco_scmp_(func, dest0, src0, src1, ...) _pco_scmp_(func, dest0, src0, src1, (struct pco_scmp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_scmp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_scmp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_SCMP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_op_main(instr, mods.tst_op_main);

   return instr;
}

#define pco_scmp(b, dest0, src0, src1, ...) _pco_scmp(b, dest0, src0, src1, (struct pco_scmp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_scmp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_scmp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_scmp_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_bcmp_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_op_main tst_op_main;
  enum pco_tst_type_main tst_type_main;
};

#define pco_bcmp_(func, dest0, src0, src1, ...) _pco_bcmp_(func, dest0, src0, src1, (struct pco_bcmp_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_bcmp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_bcmp_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_BCMP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_op_main(instr, mods.tst_op_main);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_bcmp(b, dest0, src0, src1, ...) _pco_bcmp(b, dest0, src0, src1, (struct pco_bcmp_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_bcmp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_bcmp_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_bcmp_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_bcsel_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_bcsel_(func, dest0, src0, src1, src2, ...) _pco_bcsel_(func, dest0, src0, src1, src2, (struct pco_bcsel_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_bcsel_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_bcsel_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_BCSEL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_bcsel(b, dest0, src0, src1, src2, ...) _pco_bcsel(b, dest0, src0, src1, src2, (struct pco_bcsel_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_bcsel(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_bcsel_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_bcsel_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_csel_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_op_main tst_op_main;
  enum pco_tst_type_main tst_type_main;
};

#define pco_csel_(func, dest0, src0, src1, src2, ...) _pco_csel_(func, dest0, src0, src1, src2, (struct pco_csel_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_csel_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_csel_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_CSEL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_op_main(instr, mods.tst_op_main);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_csel(b, dest0, src0, src1, src2, ...) _pco_csel(b, dest0, src0, src1, src2, (struct pco_csel_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_csel(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_csel_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_csel_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_psel_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_psel_(func, dest0, src0, src1, src2, ...) _pco_psel_(func, dest0, src0, src1, src2, (struct pco_psel_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_psel_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_psel_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_PSEL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_psel(b, dest0, src0, src1, src2, ...) _pco_psel(b, dest0, src0, src1, src2, (struct pco_psel_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_psel(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_psel_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_psel_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_psel_trig_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_psel_trig_(func, dest0, src0, src1, src2, ...) _pco_psel_trig_(func, dest0, src0, src1, src2, (struct pco_psel_trig_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_psel_trig_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_psel_trig_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_PSEL_TRIG;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_psel_trig(b, dest0, src0, src1, src2, ...) _pco_psel_trig(b, dest0, src0, src1, src2, (struct pco_psel_trig_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_psel_trig(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_psel_trig_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_psel_trig_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fsign_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fsign_(func, dest0, src0, ...) _pco_fsign_(func, dest0, src0, (struct pco_fsign_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fsign_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fsign_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FSIGN;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fsign(b, dest0, src0, ...) _pco_fsign(b, dest0, src0, (struct pco_fsign_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fsign(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fsign_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fsign_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_isign_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_isign_(func, dest0, src0, ...) _pco_isign_(func, dest0, src0, (struct pco_isign_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_isign_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_isign_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_ISIGN;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_isign(b, dest0, src0, ...) _pco_isign(b, dest0, src0, (struct pco_isign_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_isign(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_isign_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_isign_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fceil_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fceil_(func, dest0, src0, ...) _pco_fceil_(func, dest0, src0, (struct pco_fceil_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fceil_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fceil_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FCEIL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fceil(b, dest0, src0, ...) _pco_fceil(b, dest0, src0, (struct pco_fceil_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fceil(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fceil_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fceil_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_min_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_min_(func, dest0, src0, src1, ...) _pco_min_(func, dest0, src0, src1, (struct pco_min_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_min_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_min_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_MIN;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_min(b, dest0, src0, src1, ...) _pco_min(b, dest0, src0, src1, (struct pco_min_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_min(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_min_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_min_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_max_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_max_(func, dest0, src0, src1, ...) _pco_max_(func, dest0, src0, src1, (struct pco_max_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_max_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_max_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_MAX;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_max(b, dest0, src0, src1, ...) _pco_max(b, dest0, src0, src1, (struct pco_max_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_max(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_max_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_max_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_iadd32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool s;
};

#define pco_iadd32_(func, dest0, src0, src1, src2, ...) _pco_iadd32_(func, dest0, src0, src1, src2, (struct pco_iadd32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_iadd32_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_iadd32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_IADD32;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_iadd32(b, dest0, src0, src1, src2, ...) _pco_iadd32(b, dest0, src0, src1, src2, (struct pco_iadd32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_iadd32(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_iadd32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_iadd32_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_imul32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  bool s;
};

#define pco_imul32_(func, dest0, src0, src1, src2, ...) _pco_imul32_(func, dest0, src0, src1, src2, (struct pco_imul32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_imul32_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_imul32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_IMUL32;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_imul32(b, dest0, src0, src1, src2, ...) _pco_imul32(b, dest0, src0, src1, src2, (struct pco_imul32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_imul32(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_imul32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_imul32_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uadd_carry_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_uadd_carry_(func, dest0, dest1, src0, src1, ...) _pco_uadd_carry_(func, dest0, dest1, src0, src1, (struct pco_uadd_carry_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uadd_carry_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_uadd_carry_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_UADD_CARRY;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_uadd_carry(b, dest0, dest1, src0, src1, ...) _pco_uadd_carry(b, dest0, dest1, src0, src1, (struct pco_uadd_carry_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uadd_carry(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_uadd_carry_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uadd_carry_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_uadd_sat_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_uadd_sat_(func, dest0, src0, src1, src2, ...) _pco_uadd_sat_(func, dest0, src0, src1, src2, (struct pco_uadd_sat_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_uadd_sat_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_uadd_sat_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_UADD_SAT;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_uadd_sat(b, dest0, src0, src1, src2, ...) _pco_uadd_sat(b, dest0, src0, src1, src2, (struct pco_uadd_sat_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_uadd_sat(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_uadd_sat_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_uadd_sat_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_tstz_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_tstz_(func, dest0, dest1, src0, ...) _pco_tstz_(func, dest0, dest1, src0, (struct pco_tstz_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_tstz_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, struct pco_tstz_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       1);

   instr->op = PCO_OP_TSTZ;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_tstz(b, dest0, dest1, src0, ...) _pco_tstz(b, dest0, dest1, src0, (struct pco_tstz_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_tstz(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, struct pco_tstz_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_tstz_(func, dest0, dest1, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_st32_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool idf;
};

#define pco_st32_(func, src0, src1, src2, src3, src4, ...) _pco_st32_(func, src0, src1, src2, src3, src4, (struct pco_st32_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_st32_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_st32_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       5);

   instr->op = PCO_OP_ST32;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_idf(instr, mods.idf);

   return instr;
}

#define pco_st32(b, src0, src1, src2, src3, src4, ...) _pco_st32(b, src0, src1, src2, src3, src4, (struct pco_st32_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_st32(pco_builder *b, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_st32_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_st32_(func, src0, src1, src2, src3, src4, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_st32_regbl_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool idf;
};

#define pco_st32_regbl_(func, src0, src1, src2, src3, src4, ...) _pco_st32_regbl_(func, src0, src1, src2, src3, src4, (struct pco_st32_regbl_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_st32_regbl_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_st32_regbl_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       5);

   instr->op = PCO_OP_ST32_REGBL;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_idf(instr, mods.idf);

   return instr;
}

#define pco_st32_regbl(b, src0, src1, src2, src3, src4, ...) _pco_st32_regbl(b, src0, src1, src2, src3, src4, (struct pco_st32_regbl_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_st32_regbl(pco_builder *b, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, struct pco_st32_regbl_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_st32_regbl_(func, src0, src1, src2, src3, src4, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_spill_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_spill_(func, dest0, src0, src1, src2, src3, ...) _pco_spill_(func, dest0, src0, src1, src2, src3, (struct pco_spill_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_spill_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_spill_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       4);

   instr->op = PCO_OP_SPILL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_spill(b, dest0, src0, src1, src2, src3, ...) _pco_spill(b, dest0, src0, src1, src2, src3, (struct pco_spill_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_spill(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_spill_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_spill_(func, dest0, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_unspill_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
};

#define pco_unspill_(func, dest0, src0, src1, src2, ...) _pco_unspill_(func, dest0, src0, src1, src2, (struct pco_unspill_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_unspill_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_unspill_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       3);

   instr->op = PCO_OP_UNSPILL;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);

   return instr;
}

#define pco_unspill(b, dest0, src0, src1, src2, ...) _pco_unspill(b, dest0, src0, src1, src2, (struct pco_unspill_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_unspill(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_unspill_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_unspill_(func, dest0, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_iadd32_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  bool s;
};

#define pco_iadd32_atomic_(func, dest0, dest1, src0, src1, src2, ...) _pco_iadd32_atomic_(func, dest0, dest1, src0, src1, src2, (struct pco_iadd32_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_iadd32_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_iadd32_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       3);

   instr->op = PCO_OP_IADD32_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_s(instr, mods.s);

   return instr;
}

#define pco_iadd32_atomic(b, dest0, dest1, src0, src1, src2, ...) _pco_iadd32_atomic(b, dest0, dest1, src0, src1, src2, (struct pco_iadd32_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_iadd32_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_iadd32_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_iadd32_atomic_(func, dest0, dest1, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_xchg_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
};

#define pco_xchg_atomic_(func, dest0, dest1, src0, src1, ...) _pco_xchg_atomic_(func, dest0, dest1, src0, src1, (struct pco_xchg_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_xchg_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_xchg_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_XCHG_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_xchg_atomic(b, dest0, dest1, src0, src1, ...) _pco_xchg_atomic(b, dest0, dest1, src0, src1, (struct pco_xchg_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_xchg_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_xchg_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_xchg_atomic_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_cmpxchg_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_cmpxchg_atomic_(func, dest0, dest1, src0, src1, src2, ...) _pco_cmpxchg_atomic_(func, dest0, dest1, src0, src1, src2, (struct pco_cmpxchg_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_cmpxchg_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_cmpxchg_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       3);

   instr->op = PCO_OP_CMPXCHG_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_cmpxchg_atomic(b, dest0, dest1, src0, src1, src2, ...) _pco_cmpxchg_atomic(b, dest0, dest1, src0, src1, src2, (struct pco_cmpxchg_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_cmpxchg_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, struct pco_cmpxchg_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_cmpxchg_atomic_(func, dest0, dest1, src0, src1, src2, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_min_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_min_atomic_(func, dest0, dest1, src0, src1, ...) _pco_min_atomic_(func, dest0, dest1, src0, src1, (struct pco_min_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_min_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_min_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_MIN_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_min_atomic(b, dest0, dest1, src0, src1, ...) _pco_min_atomic(b, dest0, dest1, src0, src1, (struct pco_min_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_min_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_min_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_min_atomic_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_max_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  enum pco_tst_type_main tst_type_main;
};

#define pco_max_atomic_(func, dest0, dest1, src0, src1, ...) _pco_max_atomic_(func, dest0, dest1, src0, src1, (struct pco_max_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_max_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_max_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_MAX_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_tst_type_main(instr, mods.tst_type_main);

   return instr;
}

#define pco_max_atomic(b, dest0, dest1, src0, src1, ...) _pco_max_atomic(b, dest0, dest1, src0, src1, (struct pco_max_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_max_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_max_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_max_atomic_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_logical_atomic_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  enum pco_logiop logiop;
};

#define pco_logical_atomic_(func, dest0, dest1, src0, src1, ...) _pco_logical_atomic_(func, dest0, dest1, src0, src1, (struct pco_logical_atomic_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_logical_atomic_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_logical_atomic_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_LOGICAL_ATOMIC;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_logiop(instr, mods.logiop);

   return instr;
}

#define pco_logical_atomic(b, dest0, dest1, src0, src1, ...) _pco_logical_atomic(b, dest0, dest1, src0, src1, (struct pco_logical_atomic_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_logical_atomic(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_logical_atomic_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_logical_atomic_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_flush_p0_mods {
  enum pco_exec_cnd exec_cnd;
  bool end;
};

#define pco_flush_p0_(func, ...) _pco_flush_p0_(func, (struct pco_flush_p0_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_flush_p0_(pco_func *func, struct pco_flush_p0_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_FLUSH_P0;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);

   return instr;
}

#define pco_flush_p0(b, ...) _pco_flush_p0(b, (struct pco_flush_p0_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_flush_p0(pco_builder *b, struct pco_flush_p0_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_flush_p0_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_mbyp2_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_mbyp2_(func, dest0, dest1, src0, src1, ...) _pco_mbyp2_(func, dest0, dest1, src0, src1, (struct pco_mbyp2_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_mbyp2_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_mbyp2_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       2);

   instr->op = PCO_OP_MBYP2;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_mbyp2(b, dest0, dest1, src0, src1, ...) _pco_mbyp2(b, dest0, dest1, src0, src1, (struct pco_mbyp2_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_mbyp2(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, struct pco_mbyp2_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_mbyp2_(func, dest0, dest1, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fneg_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fneg_(func, dest0, src0, ...) _pco_fneg_(func, dest0, src0, (struct pco_fneg_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fneg_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fneg_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FNEG;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fneg(b, dest0, src0, ...) _pco_fneg(b, dest0, src0, (struct pco_fneg_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fneg(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fneg_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fneg_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fabs_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fabs_(func, dest0, src0, ...) _pco_fabs_(func, dest0, src0, (struct pco_fabs_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fabs_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fabs_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FABS;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fabs(b, dest0, src0, ...) _pco_fabs(b, dest0, src0, (struct pco_fabs_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fabs(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fabs_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fabs_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_fflr_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_fflr_(func, dest0, src0, ...) _pco_fflr_(func, dest0, src0, (struct pco_fflr_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_fflr_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_fflr_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FFLR;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_fflr(b, dest0, src0, ...) _pco_fflr(b, dest0, src0, (struct pco_fflr_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_fflr(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_fflr_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fflr_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_mov_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
};

#define pco_mov_(func, dest0, src0, ...) _pco_mov_(func, dest0, src0, (struct pco_mov_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_mov_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_mov_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_MOV;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_mov(b, dest0, src0, ...) _pco_mov(b, dest0, src0, (struct pco_mov_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_mov(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_mov_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_mov_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_mov_offset_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  unsigned rpt;
  enum pco_offset_sd offset_sd;
};

#define pco_mov_offset_(func, dest0, src0, src1, ...) _pco_mov_offset_(func, dest0, src0, src1, (struct pco_mov_offset_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_mov_offset_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_mov_offset_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_MOV_OFFSET;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_offset_sd(instr, mods.offset_sd);

   return instr;
}

#define pco_mov_offset(b, dest0, src0, src1, ...) _pco_mov_offset(b, dest0, src0, src1, (struct pco_mov_offset_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_mov_offset(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, struct pco_mov_offset_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_mov_offset_(func, dest0, src0, src1, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_vec_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_vec_(func, dest0, num_srcs, src, ...) _pco_vec_(func, dest0, num_srcs, src, (struct pco_vec_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_vec_(pco_func *func, pco_ref dest0, unsigned num_srcs, pco_ref *src, struct pco_vec_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       num_srcs);

   instr->op = PCO_OP_VEC;

   instr->dest[0] = dest0;
   for (unsigned s = 0; s < num_srcs; ++s)
      instr->src[s] = src[s];

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_vec(b, dest0, num_srcs, src, ...) _pco_vec(b, dest0, num_srcs, src, (struct pco_vec_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_vec(pco_builder *b, pco_ref dest0, unsigned num_srcs, pco_ref *src, struct pco_vec_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_vec_(func, dest0, num_srcs, src, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#define pco_comp_(func, dest0, src0, src1) _pco_comp_(func, dest0, src0, src1)
static
pco_instr *_pco_comp_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       2);

   instr->op = PCO_OP_COMP;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;


   return instr;
}

#define pco_comp(b, dest0, src0, src1) _pco_comp(b, dest0, src0, src1)
static inline
pco_instr *_pco_comp(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_comp_(func, dest0, src0, src1);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#define pco_fence_(func) _pco_fence_(func)
static
pco_instr *_pco_fence_(pco_func *func)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_FENCE;



   return instr;
}

#define pco_fence(b) _pco_fence(b)
static inline
pco_instr *_pco_fence(pco_builder *b)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_fence_(func);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#define pco_flush_dma_(func, dest0, src0) _pco_flush_dma_(func, dest0, src0)
static
pco_instr *_pco_flush_dma_(pco_func *func, pco_ref dest0, pco_ref src0)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_FLUSH_DMA;

   instr->dest[0] = dest0;
   instr->src[0] = src0;


   return instr;
}

#define pco_flush_dma(b, dest0, src0) _pco_flush_dma(b, dest0, src0)
static inline
pco_instr *_pco_flush_dma(pco_builder *b, pco_ref dest0, pco_ref src0)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_flush_dma_(func, dest0, src0);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_op_atomic_offset_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  unsigned rpt;
  enum pco_atom_op atom_op;
};

#define pco_op_atomic_offset_(func, dest0, dest1, src0, src1, src2, src3, ...) _pco_op_atomic_offset_(func, dest0, dest1, src0, src1, src2, src3, (struct pco_op_atomic_offset_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_op_atomic_offset_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_op_atomic_offset_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       4);

   instr->op = PCO_OP_OP_ATOMIC_OFFSET;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);
   pco_instr_set_atom_op(instr, mods.atom_op);

   return instr;
}

#define pco_op_atomic_offset(b, dest0, dest1, src0, src1, src2, src3, ...) _pco_op_atomic_offset(b, dest0, dest1, src0, src1, src2, src3, (struct pco_op_atomic_offset_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_op_atomic_offset(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_op_atomic_offset_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_op_atomic_offset_(func, dest0, dest1, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_dynidx_mods {
  enum pco_exec_cnd exec_cnd;
  unsigned rpt;
};

#define pco_dynidx_(func, dest0, dest1, src0, src1, src2, src3, ...) _pco_dynidx_(func, dest0, dest1, src0, src1, src2, src3, (struct pco_dynidx_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_dynidx_(pco_func *func, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_dynidx_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       2,
                                       4);

   instr->op = PCO_OP_DYNIDX;

   instr->dest[0] = dest0;
   instr->dest[1] = dest1;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_rpt(instr, !mods.rpt ? 1 : mods.rpt);

   return instr;
}

#define pco_dynidx(b, dest0, dest1, src0, src1, src2, src3, ...) _pco_dynidx(b, dest0, dest1, src0, src1, src2, src3, (struct pco_dynidx_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_dynidx(pco_builder *b, pco_ref dest0, pco_ref dest1, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, struct pco_dynidx_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_dynidx_(func, dest0, dest1, src0, src1, src2, src3, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_smp_dynidx_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_dim dim;
  bool proj;
  bool fcnorm;
  bool nncoords;
  enum pco_lod_mode lod_mode;
  bool pplod;
  bool tao;
  bool soo;
  bool sno;
  enum pco_sb_mode sb_mode;
  enum pco_mcu_cache_mode_ld mcu_cache_mode_ld;
  bool array;
  bool integer;
  enum pco_schedswap schedswap;
  bool f16;
};

#define pco_smp_dynidx_(func, dest0, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, ...) _pco_smp_dynidx_(func, dest0, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, (struct pco_smp_dynidx_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_smp_dynidx_(pco_func *func, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, pco_ref src6, pco_ref src7, pco_ref src8, pco_ref src9, struct pco_smp_dynidx_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       10);

   instr->op = PCO_OP_SMP_DYNIDX;

   instr->dest[0] = dest0;
   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;
   instr->src[6] = src6;
   instr->src[7] = src7;
   instr->src[8] = src8;
   instr->src[9] = src9;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_dim(instr, mods.dim);
   pco_instr_set_proj(instr, mods.proj);
   pco_instr_set_fcnorm(instr, mods.fcnorm);
   pco_instr_set_nncoords(instr, mods.nncoords);
   pco_instr_set_lod_mode(instr, mods.lod_mode);
   pco_instr_set_pplod(instr, mods.pplod);
   pco_instr_set_tao(instr, mods.tao);
   pco_instr_set_soo(instr, mods.soo);
   pco_instr_set_sno(instr, mods.sno);
   pco_instr_set_sb_mode(instr, mods.sb_mode);
   pco_instr_set_mcu_cache_mode_ld(instr, mods.mcu_cache_mode_ld);
   pco_instr_set_array(instr, mods.array);
   pco_instr_set_integer(instr, mods.integer);
   pco_instr_set_schedswap(instr, mods.schedswap);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_smp_dynidx(b, dest0, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, ...) _pco_smp_dynidx(b, dest0, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, (struct pco_smp_dynidx_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_smp_dynidx(pco_builder *b, pco_ref dest0, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, pco_ref src6, pco_ref src7, pco_ref src8, pco_ref src9, struct pco_smp_dynidx_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_smp_dynidx_(func, dest0, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_smp_wrt_dynidx_mods {
  bool olchk;
  enum pco_exec_cnd exec_cnd;
  bool end;
  bool atom;
  enum pco_dim dim;
  bool proj;
  bool fcnorm;
  bool nncoords;
  enum pco_lod_mode lod_mode;
  bool pplod;
  bool tao;
  bool soo;
  bool sno;
  enum pco_sb_mode sb_mode;
  enum pco_mcu_cache_mode_st mcu_cache_mode_st;
  bool array;
  bool integer;
  enum pco_schedswap schedswap;
  bool f16;
};

#define pco_smp_wrt_dynidx_(func, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, ...) _pco_smp_wrt_dynidx_(func, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, (struct pco_smp_wrt_dynidx_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_smp_wrt_dynidx_(pco_func *func, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, pco_ref src6, pco_ref src7, pco_ref src8, pco_ref src9, struct pco_smp_wrt_dynidx_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       10);

   instr->op = PCO_OP_SMP_WRT_DYNIDX;

   instr->src[0] = src0;
   instr->src[1] = src1;
   instr->src[2] = src2;
   instr->src[3] = src3;
   instr->src[4] = src4;
   instr->src[5] = src5;
   instr->src[6] = src6;
   instr->src[7] = src7;
   instr->src[8] = src8;
   instr->src[9] = src9;

   pco_instr_set_olchk(instr, mods.olchk);
   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_end(instr, mods.end);
   pco_instr_set_atom(instr, mods.atom);
   pco_instr_set_dim(instr, mods.dim);
   pco_instr_set_proj(instr, mods.proj);
   pco_instr_set_fcnorm(instr, mods.fcnorm);
   pco_instr_set_nncoords(instr, mods.nncoords);
   pco_instr_set_lod_mode(instr, mods.lod_mode);
   pco_instr_set_pplod(instr, mods.pplod);
   pco_instr_set_tao(instr, mods.tao);
   pco_instr_set_soo(instr, mods.soo);
   pco_instr_set_sno(instr, mods.sno);
   pco_instr_set_sb_mode(instr, mods.sb_mode);
   pco_instr_set_mcu_cache_mode_st(instr, mods.mcu_cache_mode_st);
   pco_instr_set_array(instr, mods.array);
   pco_instr_set_integer(instr, mods.integer);
   pco_instr_set_schedswap(instr, mods.schedswap);
   pco_instr_set_f16(instr, mods.f16);

   return instr;
}

#define pco_smp_wrt_dynidx(b, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, ...) _pco_smp_wrt_dynidx(b, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, (struct pco_smp_wrt_dynidx_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_smp_wrt_dynidx(pco_builder *b, pco_ref src0, pco_ref src1, pco_ref src2, pco_ref src3, pco_ref src4, pco_ref src5, pco_ref src6, pco_ref src7, pco_ref src8, pco_ref src9, struct pco_smp_wrt_dynidx_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_smp_wrt_dynidx_(func, src0, src1, src2, src3, src4, src5, src6, src7, src8, src9, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_break_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_break_(func, ...) _pco_break_(func, (struct pco_break_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_break_(pco_func *func, struct pco_break_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_BREAK;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_break(b, ...) _pco_break(b, (struct pco_break_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_break(pco_builder *b, struct pco_break_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_break_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_continue_mods {
  enum pco_exec_cnd exec_cnd;
};

#define pco_continue_(func, ...) _pco_continue_(func, (struct pco_continue_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_continue_(pco_func *func, struct pco_continue_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       0,
                                       0);

   instr->op = PCO_OP_CONTINUE;


   pco_instr_set_exec_cnd(instr, mods.exec_cnd);

   return instr;
}

#define pco_continue(b, ...) _pco_continue(b, (struct pco_continue_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_continue(pco_builder *b, struct pco_continue_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_continue_(func, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

struct pco_vote_mods {
  enum pco_exec_cnd exec_cnd;
  enum pco_vote_op vote_op;
};

#define pco_vote_(func, dest0, src0, ...) _pco_vote_(func, dest0, src0, (struct pco_vote_mods){0, ##__VA_ARGS__})
static
pco_instr *_pco_vote_(pco_func *func, pco_ref dest0, pco_ref src0, struct pco_vote_mods mods)
{
   pco_instr *instr = pco_instr_create(func,
                                       1,
                                       1);

   instr->op = PCO_OP_VOTE;

   instr->dest[0] = dest0;
   instr->src[0] = src0;

   pco_instr_set_exec_cnd(instr, mods.exec_cnd);
   pco_instr_set_vote_op(instr, mods.vote_op);

   return instr;
}

#define pco_vote(b, dest0, src0, ...) _pco_vote(b, dest0, src0, (struct pco_vote_mods){0, ##__VA_ARGS__})
static inline
pco_instr *_pco_vote(pco_builder *b, pco_ref dest0, pco_ref src0, struct pco_vote_mods mods)
{
   pco_func *func = pco_cursor_func(b->cursor);
   pco_instr *instr = _pco_vote_(func, dest0, src0, mods);
   pco_builder_insert_instr(b, instr);
   return instr;
}

#endif /* PCO_BUILDER_OPS_H */

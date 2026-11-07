/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PCO_MAP_H
#define PCO_MAP_H

/**
 * \file pco_map.h
 *
 * \brief PCO mapping definitions.
 */

#include "pco_builder.h"
#include "pco_common.h"
#include "pco_internal.h"
#include "pco_isa.h"
#include "pco_ops.h"
#include "util/macros.h"

#include <stdbool.h>

/* Enum mappings. */
static inline
enum pco_cc pco_map_exec_cnd_to_cc(enum pco_exec_cnd val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_EXEC_CND_E1_ZX:
      to = PCO_CC_E1_ZX;
      break;

   case PCO_EXEC_CND_E1_Z1:
      to = PCO_CC_E1_Z1;
      break;

   case PCO_EXEC_CND_EX_ZX:
      to = PCO_CC_EX_ZX;
      break;

   case PCO_EXEC_CND_E1_Z0:
      to = PCO_CC_E1_Z0;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_regbank pco_map_reg_class_to_regbank(enum pco_reg_class val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_REG_CLASS_TEMP:
      to = PCO_REGBANK_TEMP;
      break;

   case PCO_REG_CLASS_VTXIN:
      to = PCO_REGBANK_VTXIN;
      break;

   case PCO_REG_CLASS_COEFF:
      to = PCO_REGBANK_COEFF;
      break;

   case PCO_REG_CLASS_SHARED:
      to = PCO_REGBANK_SHARED;
      break;

   case PCO_REG_CLASS_INDEX:
      to = PCO_REGBANK_IDX0;
      break;

   case PCO_REG_CLASS_SPEC:
      to = PCO_REGBANK_SPECIAL;
      break;

   case PCO_REG_CLASS_INTERN:
      to = PCO_REGBANK_SPECIAL;
      break;

   case PCO_REG_CLASS_CONST:
      to = PCO_REGBANK_SPECIAL;
      break;

   case PCO_REG_CLASS_PIXOUT:
      to = PCO_REGBANK_SPECIAL;
      break;

   case PCO_REG_CLASS_GLOBAL:
      to = PCO_REGBANK_SPECIAL;
      break;

   case PCO_REG_CLASS_SLOT:
      to = PCO_REGBANK_SPECIAL;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_idxbank pco_map_reg_class_to_idxbank(enum pco_reg_class val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_REG_CLASS_TEMP:
      to = PCO_IDXBANK_TEMP;
      break;

   case PCO_REG_CLASS_VTXIN:
      to = PCO_IDXBANK_VTXIN;
      break;

   case PCO_REG_CLASS_COEFF:
      to = PCO_IDXBANK_COEFF;
      break;

   case PCO_REG_CLASS_SHARED:
      to = PCO_IDXBANK_SHARED;
      break;

   case PCO_REG_CLASS_INDEX:
      to = PCO_IDXBANK_IDX;
      break;

   case PCO_REG_CLASS_PIXOUT:
      to = PCO_IDXBANK_PIXOUT;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is0_sel pco_map_io_to_is0_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_S0:
      to = PCO_IS0_SEL_S0;
      break;

   case PCO_IO_S1:
      to = PCO_IS0_SEL_S1;
      break;

   case PCO_IO_S2:
      to = PCO_IS0_SEL_S2;
      break;

   case PCO_IO_S3:
      to = PCO_IS0_SEL_S3;
      break;

   case PCO_IO_S4:
      to = PCO_IS0_SEL_S4;
      break;

   case PCO_IO_S5:
      to = PCO_IS0_SEL_S5;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is1_sel pco_map_io_to_is1_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_FT0:
      to = PCO_IS1_SEL_FT0;
      break;

   case PCO_IO_FTE:
      to = PCO_IS1_SEL_FTE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is2_sel pco_map_io_to_is2_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_FT1:
      to = PCO_IS2_SEL_FT1;
      break;

   case PCO_IO_FTE:
      to = PCO_IS2_SEL_FTE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is3_sel pco_map_io_to_is3_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_FT0:
      to = PCO_IS3_SEL_FT0;
      break;

   case PCO_IO_FT1:
      to = PCO_IS3_SEL_FT1;
      break;

   case PCO_IO_FTE:
      to = PCO_IS3_SEL_FTE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is4_sel pco_map_io_to_is4_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_FT0:
      to = PCO_IS4_SEL_FT0;
      break;

   case PCO_IO_FT1:
      to = PCO_IS4_SEL_FT1;
      break;

   case PCO_IO_FT2:
      to = PCO_IS4_SEL_FT2;
      break;

   case PCO_IO_FTE:
      to = PCO_IS4_SEL_FTE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_is5_sel pco_map_io_to_is5_sel(enum pco_io val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_IO_FT0:
      to = PCO_IS5_SEL_FT0;
      break;

   case PCO_IO_FT1:
      to = PCO_IS5_SEL_FT1;
      break;

   case PCO_IO_FT2:
      to = PCO_IS5_SEL_FT2;
      break;

   case PCO_IO_FTE:
      to = PCO_IS5_SEL_FTE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_iter_mode pco_map_itr_mode_to_iter_mode(enum pco_itr_mode val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_ITR_MODE_PIXEL:
      to = PCO_ITER_MODE_PIXEL;
      break;

   case PCO_ITR_MODE_SAMPLE:
      to = PCO_ITER_MODE_SAMPLE;
      break;

   case PCO_ITR_MODE_CENTROID:
      to = PCO_ITER_MODE_CENTROID;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_pck_format pco_map_pck_fmt_to_pck_format(enum pco_pck_fmt val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_PCK_FMT_U8888:
      to = PCO_PCK_FORMAT_U8888;
      break;

   case PCO_PCK_FMT_S8888:
      to = PCO_PCK_FORMAT_S8888;
      break;

   case PCO_PCK_FMT_O8888:
      to = PCO_PCK_FORMAT_O8888;
      break;

   case PCO_PCK_FMT_U1616:
      to = PCO_PCK_FORMAT_U1616;
      break;

   case PCO_PCK_FMT_S1616:
      to = PCO_PCK_FORMAT_S1616;
      break;

   case PCO_PCK_FMT_O1616:
      to = PCO_PCK_FORMAT_O1616;
      break;

   case PCO_PCK_FMT_U32:
      to = PCO_PCK_FORMAT_U32;
      break;

   case PCO_PCK_FMT_S32:
      to = PCO_PCK_FORMAT_S32;
      break;

   case PCO_PCK_FMT_U1010102:
      to = PCO_PCK_FORMAT_U1010102;
      break;

   case PCO_PCK_FMT_S1010102:
      to = PCO_PCK_FORMAT_S1010102;
      break;

   case PCO_PCK_FMT_U111110:
      to = PCO_PCK_FORMAT_U111110;
      break;

   case PCO_PCK_FMT_S111110:
      to = PCO_PCK_FORMAT_S111110;
      break;

   case PCO_PCK_FMT_F111110:
      to = PCO_PCK_FORMAT_F111110;
      break;

   case PCO_PCK_FMT_F16F16:
      to = PCO_PCK_FORMAT_F16F16;
      break;

   case PCO_PCK_FMT_F32:
      to = PCO_PCK_FORMAT_F32;
      break;

   case PCO_PCK_FMT_COV:
      to = PCO_PCK_FORMAT_COV;
      break;

   case PCO_PCK_FMT_U565U565:
      to = PCO_PCK_FORMAT_U565U565;
      break;

   case PCO_PCK_FMT_D24S8:
      to = PCO_PCK_FORMAT_D24S8;
      break;

   case PCO_PCK_FMT_S8D24:
      to = PCO_PCK_FORMAT_S8D24;
      break;

   case PCO_PCK_FMT_F32_MASK:
      to = PCO_PCK_FORMAT_F32_MASK;
      break;

   case PCO_PCK_FMT_2F10F10F10:
      to = PCO_PCK_FORMAT_2F10F10F10;
      break;

   case PCO_PCK_FMT_S8888OGL:
      to = PCO_PCK_FORMAT_S8888OGL;
      break;

   case PCO_PCK_FMT_S1616OGL:
      to = PCO_PCK_FORMAT_S1616OGL;
      break;

   case PCO_PCK_FMT_ZERO:
      to = PCO_PCK_FORMAT_ZERO;
      break;

   case PCO_PCK_FMT_ONE:
      to = PCO_PCK_FORMAT_ONE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_sched_ctrl pco_map_sched_to_sched_ctrl(enum pco_sched val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_SCHED_NONE:
      to = PCO_SCHED_CTRL_NONE;
      break;

   case PCO_SCHED_SWAP:
      to = PCO_SCHED_CTRL_SWAP;
      break;

   case PCO_SCHED_WDF:
      to = PCO_SCHED_CTRL_WDF;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_cachemode_ld pco_map_mcu_cache_mode_ld_to_cachemode_ld(enum pco_mcu_cache_mode_ld val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_MCU_CACHE_MODE_LD_NORMAL:
      to = PCO_CACHEMODE_LD_NORMAL;
      break;

   case PCO_MCU_CACHE_MODE_LD_BYPASS:
      to = PCO_CACHEMODE_LD_BYPASS;
      break;

   case PCO_MCU_CACHE_MODE_LD_FORCE_LINE_FILL:
      to = PCO_CACHEMODE_LD_FORCE_LINE_FILL;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_cachemode_st pco_map_mcu_cache_mode_st_to_cachemode_st(enum pco_mcu_cache_mode_st val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_MCU_CACHE_MODE_ST_WRITE_THROUGH:
      to = PCO_CACHEMODE_ST_WRITE_THROUGH;
      break;

   case PCO_MCU_CACHE_MODE_ST_WRITE_BACK:
      to = PCO_CACHEMODE_ST_WRITE_BACK;
      break;

   case PCO_MCU_CACHE_MODE_ST_LAZY_WRITE_BACK:
      to = PCO_CACHEMODE_ST_WRITE_BACK_LAZY;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_cachemode_ld_st pco_map_mcu_cache_mode_ld_to_cachemode_ld_st(enum pco_mcu_cache_mode_ld val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_MCU_CACHE_MODE_LD_NORMAL:
      to = PCO_CACHEMODE_LD_ST_NORMAL;
      break;

   case PCO_MCU_CACHE_MODE_LD_BYPASS:
      to = PCO_CACHEMODE_LD_ST_BYPASS;
      break;

   case PCO_MCU_CACHE_MODE_LD_FORCE_LINE_FILL:
      to = PCO_CACHEMODE_LD_ST_FORCE_LINE_FILL;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_cachemode_ld_st pco_map_mcu_cache_mode_st_to_cachemode_ld_st(enum pco_mcu_cache_mode_st val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_MCU_CACHE_MODE_ST_WRITE_THROUGH:
      to = PCO_CACHEMODE_LD_ST_WRITE_THROUGH;
      break;

   case PCO_MCU_CACHE_MODE_ST_WRITE_BACK:
      to = PCO_CACHEMODE_LD_ST_WRITE_BACK;
      break;

   case PCO_MCU_CACHE_MODE_ST_LAZY_WRITE_BACK:
      to = PCO_CACHEMODE_LD_ST_WRITE_BACK_LAZY;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_tst_op pco_map_tst_op_main_to_tst_op(enum pco_tst_op_main val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_TST_OP_MAIN_ZERO:
      to = PCO_TST_OP_Z;
      break;

   case PCO_TST_OP_MAIN_GZERO:
      to = PCO_TST_OP_GZ;
      break;

   case PCO_TST_OP_MAIN_GEZERO:
      to = PCO_TST_OP_GEZ;
      break;

   case PCO_TST_OP_MAIN_CARRY:
      to = PCO_TST_OP_C;
      break;

   case PCO_TST_OP_MAIN_EQUAL:
      to = PCO_TST_OP_E;
      break;

   case PCO_TST_OP_MAIN_GREATER:
      to = PCO_TST_OP_G;
      break;

   case PCO_TST_OP_MAIN_GEQUAL:
      to = PCO_TST_OP_GE;
      break;

   case PCO_TST_OP_MAIN_NOTEQUAL:
      to = PCO_TST_OP_NE;
      break;

   case PCO_TST_OP_MAIN_LESS:
      to = PCO_TST_OP_L;
      break;

   case PCO_TST_OP_MAIN_LEQUAL:
      to = PCO_TST_OP_LE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_tst_type pco_map_tst_type_main_to_tst_type(enum pco_tst_type_main val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_TST_TYPE_MAIN_F32:
      to = PCO_TST_TYPE_F32;
      break;

   case PCO_TST_TYPE_MAIN_U16:
      to = PCO_TST_TYPE_U16;
      break;

   case PCO_TST_TYPE_MAIN_S16:
      to = PCO_TST_TYPE_S16;
      break;

   case PCO_TST_TYPE_MAIN_U8:
      to = PCO_TST_TYPE_U8;
      break;

   case PCO_TST_TYPE_MAIN_S8:
      to = PCO_TST_TYPE_S8;
      break;

   case PCO_TST_TYPE_MAIN_U32:
      to = PCO_TST_TYPE_U32;
      break;

   case PCO_TST_TYPE_MAIN_S32:
      to = PCO_TST_TYPE_S32;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_logical_op pco_map_logiop_to_logical_op(enum pco_logiop val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_LOGIOP_OR:
      to = PCO_LOGICAL_OP_OR;
      break;

   case PCO_LOGIOP_AND:
      to = PCO_LOGICAL_OP_AND;
      break;

   case PCO_LOGIOP_XOR:
      to = PCO_LOGICAL_OP_XOR;
      break;

   case PCO_LOGIOP_NOR:
      to = PCO_LOGICAL_OP_NOR;
      break;

   case PCO_LOGIOP_NAND:
      to = PCO_LOGICAL_OP_NAND;
      break;

   case PCO_LOGIOP_XNOR:
      to = PCO_LOGICAL_OP_XNOR;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_shift2_op pco_map_shiftop_to_shift2_op(enum pco_shiftop val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_SHIFTOP_LSL:
      to = PCO_SHIFT2_OP_LSL;
      break;

   case PCO_SHIFTOP_SHR:
      to = PCO_SHIFT2_OP_SHR;
      break;

   case PCO_SHIFTOP_ROL:
      to = PCO_SHIFT2_OP_ROL;
      break;

   case PCO_SHIFTOP_CPS:
      to = PCO_SHIFT2_OP_CPS;
      break;

   case PCO_SHIFTOP_ASR_TWB:
      to = PCO_SHIFT2_OP_ASR_TWB;
      break;

   case PCO_SHIFTOP_ASR_PWB:
      to = PCO_SHIFT2_OP_ASR_PWB;
      break;

   case PCO_SHIFTOP_ASR_MTB:
      to = PCO_SHIFT2_OP_ASR_MTB;
      break;

   case PCO_SHIFTOP_ASR_FTB:
      to = PCO_SHIFT2_OP_ASR_FTB;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_upck_elem pco_map_elem_to_upck_elem(enum pco_elem val)
{
   uint64_t to = 0;

   if (!val)
      return 0;

   switch (val) {
   case PCO_ELEM_E0:
      to = PCO_UPCK_ELEM_E0;
      break;

   case PCO_ELEM_E1:
      to = PCO_UPCK_ELEM_E1;
      break;

   case PCO_ELEM_E2:
      to = PCO_UPCK_ELEM_E2;
      break;

   case PCO_ELEM_E3:
      to = PCO_UPCK_ELEM_E3;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_maskw0 pco_map_elem_to_maskw0(enum pco_elem val)
{
   uint64_t to = 0;

   if (!val)
      return PCO_MASKW0_E0 | PCO_MASKW0_E1 | PCO_MASKW0_E2 | PCO_MASKW0_E3;

   u_foreach_bit64 (b, val) {
      switch (b) {
      case PCO_ELEM_E0:
         to |= PCO_MASKW0_E0;
         break;

      case PCO_ELEM_E1:
         to |= PCO_MASKW0_E1;
         break;

      case PCO_ELEM_E2:
         to |= PCO_MASKW0_E2;
         break;

      case PCO_ELEM_E3:
         to |= PCO_MASKW0_E3;
         break;

      default:
         UNREACHABLE("");
      }
   }

   return to;
}

static inline
enum pco_pcnd pco_map_cnd_to_pcnd(enum pco_cnd val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_CND_ALWAYS:
      to = PCO_PCND_ALWAYS;
      break;

   case PCO_CND_P0_TRUE:
      to = PCO_PCND_P0_TRUE;
      break;

   case PCO_CND_NEVER:
      to = PCO_PCND_NEVER;
      break;

   case PCO_CND_P0_FALSE:
      to = PCO_PCND_P0_FALSE;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_bpred pco_map_branch_cnd_to_bpred(enum pco_branch_cnd val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_BRANCH_CND_EXEC_COND:
      to = PCO_BPRED_CC;
      break;

   case PCO_BRANCH_CND_ALLINST:
      to = PCO_BPRED_ALLP;
      break;

   case PCO_BRANCH_CND_ANYINST:
      to = PCO_BPRED_ANYP;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_atomic_op pco_map_atom_op_to_atomic_op(enum pco_atom_op val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_ATOM_OP_ADD:
      to = PCO_ATOMIC_OP_ADD;
      break;

   case PCO_ATOM_OP_SUB:
      to = PCO_ATOMIC_OP_SUB;
      break;

   case PCO_ATOM_OP_XCHG:
      to = PCO_ATOMIC_OP_XCHG;
      break;

   case PCO_ATOM_OP_UMIN:
      to = PCO_ATOMIC_OP_UMIN;
      break;

   case PCO_ATOM_OP_IMIN:
      to = PCO_ATOMIC_OP_IMIN;
      break;

   case PCO_ATOM_OP_UMAX:
      to = PCO_ATOMIC_OP_UMAX;
      break;

   case PCO_ATOM_OP_IMAX:
      to = PCO_ATOMIC_OP_IMAX;
      break;

   case PCO_ATOM_OP_AND:
      to = PCO_ATOMIC_OP_AND;
      break;

   case PCO_ATOM_OP_OR:
      to = PCO_ATOMIC_OP_OR;
      break;

   case PCO_ATOM_OP_XOR:
      to = PCO_ATOMIC_OP_XOR;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_red_type pco_map_fred_type_to_red_type(enum pco_fred_type val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_FRED_TYPE_SIN:
      to = PCO_RED_TYPE_SIN;
      break;

   case PCO_FRED_TYPE_COS:
      to = PCO_RED_TYPE_COS;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_red_part pco_map_fred_part_to_red_part(enum pco_fred_part val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_FRED_PART_A:
      to = PCO_RED_PART_A;
      break;

   case PCO_FRED_PART_B:
      to = PCO_RED_PART_B;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_dmn pco_map_dim_to_dmn(enum pco_dim val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_DIM_1D:
      to = PCO_DMN_1D;
      break;

   case PCO_DIM_2D:
      to = PCO_DMN_2D;
      break;

   case PCO_DIM_3D:
      to = PCO_DMN_3D;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_lodm pco_map_lod_mode_to_lodm(enum pco_lod_mode val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_LOD_MODE_NORMAL:
      to = PCO_LODM_NORMAL;
      break;

   case PCO_LOD_MODE_BIAS:
      to = PCO_LODM_BIAS;
      break;

   case PCO_LOD_MODE_REPLACE:
      to = PCO_LODM_REPLACE;
      break;

   case PCO_LOD_MODE_GRADIENTS:
      to = PCO_LODM_GRADIENTS;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_sbmode pco_map_sb_mode_to_sbmode(enum pco_sb_mode val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_SB_MODE_NONE:
      to = PCO_SBMODE_NONE;
      break;

   case PCO_SB_MODE_RAWDATA:
      to = PCO_SBMODE_RAWDATA;
      break;

   case PCO_SB_MODE_COEFFS:
      to = PCO_SBMODE_COEFFS;
      break;

   case PCO_SB_MODE_BOTH:
      to = PCO_SBMODE_BOTH;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_sched_ctrl pco_map_schedswap_to_sched_ctrl(enum pco_schedswap val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_SCHEDSWAP_NONE:
      to = PCO_SCHED_CTRL_NONE;
      break;

   case PCO_SCHEDSWAP_SWAP:
      to = PCO_SCHED_CTRL_SWAP;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_lr pco_map_mutex_op_to_lr(enum pco_mutex_op val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_MUTEX_OP_RELEASE:
      to = PCO_LR_RELEASE;
      break;

   case PCO_MUTEX_OP_RELEASE_SLEEP:
      to = PCO_LR_RELEASE_SLEEP;
      break;

   case PCO_MUTEX_OP_RELEASE_WAKEUP:
      to = PCO_LR_RELEASE_WAKEUP;
      break;

   case PCO_MUTEX_OP_LOCK:
      to = PCO_LR_LOCK;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_msk_mode pco_map_savmsk_mode_to_msk_mode(enum pco_savmsk_mode val)
{
   uint64_t to = 0;

   switch (val) {
   case PCO_SAVMSK_MODE_VM:
      to = PCO_MSK_MODE_VM;
      break;

   case PCO_SAVMSK_MODE_ICM:
      to = PCO_MSK_MODE_ICM;
      break;

   case PCO_SAVMSK_MODE_ICMOC:
      to = PCO_MSK_MODE_ICMOC;
      break;

   case PCO_SAVMSK_MODE_ICMI:
      to = PCO_MSK_MODE_ICMI;
      break;

   case PCO_SAVMSK_MODE_CAXY:
      to = PCO_MSK_MODE_CAXY;
      break;

   default:
      UNREACHABLE("");
   }

   return to;
}

static inline
enum pco_regbank pco_map_reg_bank(pco_ref ref)
{
   if (pco_ref_is_idx_reg(ref)) {
      return PCO_REGBANK_IDX0 + ref.idx_reg.num;
   }

   return pco_map_reg_class_to_regbank(pco_ref_get_reg_class(ref));
}

static inline
unsigned pco_map_reg_bank_bits(pco_ref ref)
{
   return util_last_bit(pco_map_reg_bank(ref));
}

static inline
enum pco_idxbank pco_map_idx_bank(pco_ref ref)
{
   assert(pco_ref_is_idx_reg(ref));
   return pco_map_reg_class_to_idxbank(ref.reg_class);
}

static inline unsigned pco_map_reg_index(pco_ref ref)
{
   if (pco_ref_is_reg(ref)) {
      switch (ref.reg_class) {
      case PCO_REG_CLASS_TEMP:
      case PCO_REG_CLASS_VTXIN:
      case PCO_REG_CLASS_COEFF:
      case PCO_REG_CLASS_SHARED:
      case PCO_REG_CLASS_SPEC:
      case PCO_REG_CLASS_CONST:
         return ref.val;

      case PCO_REG_CLASS_INTERN:
         return ref.val + PCO_SR_INTL0;

      case PCO_REG_CLASS_PIXOUT:
         if (ref.val >= 4)
            return ref.val + PCO_SR_PIXOUT4 - 4;
         else
            return ref.val + PCO_SR_PIXOUT0;

      case PCO_REG_CLASS_GLOBAL:
         return ref.val + PCO_SR_GLOBAL0;

      case PCO_REG_CLASS_SLOT:
         return (7 - ref.val) + PCO_SR_SLOT7;

      default:
         break;
      }
   } else if (pco_ref_is_idx_reg(ref)) {
      return pco_map_idx_bank(ref) | (ref.idx_reg.offset << 3);
   }

   UNREACHABLE("");
}

static inline unsigned pco_map_reg_index_bits(pco_ref ref)
{
   if (pco_ref_is_reg(ref))
      return util_last_bit(pco_map_reg_index(ref));
   else if (pco_ref_is_idx_reg(ref))
      return 11;

   UNREACHABLE("");
}

static inline
enum pco_igrp_hdr_variant pco_igrp_hdr_variant(pco_igrp *igrp)
{
   if (igrp->hdr.alutype == PCO_ALUTYPE_BITWISE)
      return PCO_IGRP_HDR_BITWISE;
   else if (igrp->hdr.alutype == PCO_ALUTYPE_CONTROL)
      return PCO_IGRP_HDR_CONTROL;
   else
      assert(igrp->hdr.alutype == PCO_ALUTYPE_MAIN);

   if (!igrp->hdr.end && !igrp->hdr.atom && !igrp->hdr.rpt && !igrp->hdr.cc)
      return PCO_IGRP_HDR_MAIN_BRIEF;

   return PCO_IGRP_HDR_MAIN;
}

static inline
enum pco_src_variant pco_igrp_src_variant(const pco_igrp *igrp,
                                          bool is_upper)
{
   unsigned offset = is_upper ? ROGUE_ALU_INPUT_GROUP_SIZE : 0;

   pco_ref sA = igrp->srcs.s[0 + offset];
   pco_ref sB = igrp->srcs.s[1 + offset];
   pco_ref sC = igrp->srcs.s[2 + offset];
   pco_ref mux = is_upper ? pco_ref_null() : igrp->iss.is[0];

   bool sA_set = !pco_ref_is_null(sA);
   bool sB_set = !pco_ref_is_null(sB);
   bool sC_set = !pco_ref_is_null(sC);
   bool mux_set = !pco_ref_is_null(mux);

   int sbA_bits = sA_set ? pco_map_reg_bank_bits(sA) : -1;
   int sA_bits = sA_set ? pco_map_reg_index_bits(sA) : -1;

   int sbB_bits = sB_set ? pco_map_reg_bank_bits(sB) : -1;
   int sB_bits = sB_set ? pco_map_reg_index_bits(sB) : -1;

   int sbC_bits = sC_set ? pco_map_reg_bank_bits(sC) : -1;
   int sC_bits = sC_set ? pco_map_reg_index_bits(sC) : -1;

   int mux_bits = mux_set ? util_last_bit(pco_map_io_to_is0_sel(pco_ref_get_io(mux))) : -1;

   bool is_ctrl = igrp->hdr.alutype == PCO_ALUTYPE_CONTROL;
   bool no_srcs_set = !sA_set && !sB_set && !sC_set && !mux_set;
   if (is_ctrl && no_srcs_set)
      return PCO_SRC_NONE;

   if (!is_upper &&
            sbA_bits <= 1 && sA_bits <= 6 &&
            sbB_bits == -1 && sB_bits == -1 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_1LO_1B6I;
   }
   else if (!is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits == -1 && sB_bits == -1 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits <= 2) {
      return PCO_SRC_1LO_3B11I_2M;
   }
   else if (!is_upper &&
            sbA_bits <= 1 && sA_bits <= 6 &&
            sbB_bits <= 1 && sB_bits <= 5 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_2LO_1B6I_1B5I;
   }
   else if (!is_upper &&
            sbA_bits <= 2 && sA_bits <= 7 &&
            sbB_bits <= 2 && sB_bits <= 7 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits <= 2) {
      return PCO_SRC_2LO_2B7I_2B7I_2M;
   }
   else if (!is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits <= 3) {
      return PCO_SRC_2LO_3B11I_2B8I_3M;
   }
   else if (!is_upper &&
            sbA_bits <= 2 && sA_bits <= 7 &&
            sbB_bits <= 2 && sB_bits <= 7 &&
            sbC_bits <= 2 && sC_bits <= 6 &&
            mux_bits <= 2) {
      return PCO_SRC_3LO_2B7I_2B7I_2B6I_2M;
   }
   else if (!is_upper &&
            sbA_bits <= 3 && sA_bits <= 8 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits <= 3 && sC_bits <= 8 &&
            mux_bits <= 3) {
      return PCO_SRC_3LO_3B8I_2B8I_3B8I_3M;
   }
   else if (!is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits <= 3 && sC_bits <= 11 &&
            mux_bits <= 3) {
      return PCO_SRC_3LO_3B11I_2B8I_3B11I_3M;
   }
   else if (is_upper &&
            sbA_bits <= 1 && sA_bits <= 6 &&
            sbB_bits == -1 && sB_bits == -1 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_1UP_1B6I;
   }
   else if (is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits == -1 && sB_bits == -1 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_1UP_3B11I;
   }
   else if (is_upper &&
            sbA_bits <= 1 && sA_bits <= 6 &&
            sbB_bits <= 1 && sB_bits <= 5 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_2UP_1B6I_1B5I;
   }
   else if (is_upper &&
            sbA_bits <= 2 && sA_bits <= 7 &&
            sbB_bits <= 2 && sB_bits <= 7 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_2UP_2B7I_2B7I;
   }
   else if (is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits == -1 && sC_bits == -1 &&
            mux_bits == -1) {
      return PCO_SRC_2UP_3B11I_2B8I;
   }
   else if (is_upper &&
            sbA_bits <= 2 && sA_bits <= 7 &&
            sbB_bits <= 2 && sB_bits <= 7 &&
            sbC_bits <= 2 && sC_bits <= 6 &&
            mux_bits == -1) {
      return PCO_SRC_3UP_2B7I_2B7I_2B6I;
   }
   else if (is_upper &&
            sbA_bits <= 3 && sA_bits <= 8 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits <= 2 && sC_bits <= 8 &&
            mux_bits == -1) {
      return PCO_SRC_3UP_3B8I_2B8I_2B8I;
   }
   else if (is_upper &&
            sbA_bits <= 3 && sA_bits <= 11 &&
            sbB_bits <= 2 && sB_bits <= 8 &&
            sbC_bits <= 2 && sC_bits <= 8 &&
            mux_bits == -1) {
      return PCO_SRC_3UP_3B11I_2B8I_2B8I;
   }

   UNREACHABLE("");
}

static inline
enum pco_iss_variant pco_igrp_iss_variant(const pco_igrp *igrp)
{
   if (igrp->hdr.alutype == PCO_ALUTYPE_MAIN)
      return PCO_ISS_ISS;

   return PCO_ISS_NONE;
}

static inline
enum pco_dst_variant pco_igrp_dest_variant(pco_igrp *igrp)
{
   pco_ref w0 = igrp->dests.w[0];
   pco_ref w1 = igrp->dests.w[1];

   bool w0_set = !pco_ref_is_null(w0);
   bool w1_set = !pco_ref_is_null(w1);

   bool no_dsts = !w0_set && !w1_set;
   bool one_dest = w0_set != w1_set;
   bool dual_dsts = w0_set && w1_set;

   if (no_dsts)
      return PCO_DST_NONE;

   int db0_bits = w0_set ? pco_map_reg_bank_bits(w0) : -1;
   int d0_bits = w0_set ? pco_map_reg_index_bits(w0) : -1;

   int db1_bits = w1_set ? pco_map_reg_bank_bits(w1) : -1;
   int d1_bits = w1_set ? pco_map_reg_index_bits(w1) : -1;

   int dbN_bits = w0_set ? db0_bits : db1_bits;
   int dN_bits = w0_set ? d0_bits : d1_bits;

   if (one_dest) {
      db0_bits = dbN_bits;
      d0_bits = dN_bits;

      db1_bits = -1;
      d1_bits = -1;
   }

   if (!dual_dsts &&
        db0_bits <= 1 && d0_bits <= 6 &&
        db1_bits == -1 && d1_bits == -1) {
      return PCO_DST_1_1B6I;
   }
   else if (!dual_dsts &&
        db0_bits <= 3 && d0_bits <= 11 &&
        db1_bits == -1 && d1_bits == -1) {
      return PCO_DST_1_3B11I;
   }
   else if (dual_dsts &&
        db0_bits <= 1 && d0_bits <= 7 &&
        db1_bits <= 1 && d1_bits <= 6) {
      return PCO_DST_2_1B7I_1B6I;
   }
   else if (dual_dsts &&
        db0_bits <= 3 && d0_bits <= 8 &&
        db1_bits <= 3 && d1_bits <= 8) {
      return PCO_DST_2_3B8I_3B8I;
   }
   else if (dual_dsts &&
        db0_bits <= 3 && d0_bits <= 11 &&
        db1_bits <= 3 && d1_bits <= 11) {
      return PCO_DST_2_3B11I_3B11I;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fadd_variant(pco_instr *instr)
{
   return PCO_MAIN_FADD;
}

static inline
unsigned pco_fmul_variant(pco_instr *instr)
{
   return PCO_MAIN_FMUL;
}

static inline
unsigned pco_fmad_variant(pco_instr *instr)
{
   if (pco_instr_get_mod(instr, PCO_OP_MOD_LP) == false && instr->src[1].abs == false && instr->src[1].neg == false && instr->src[2].flr == false && instr->src[2].abs == false)
      return PCO_MAIN_FMAD;

   return PCO_MAIN_FMAD_EXT;
}

static inline
unsigned pco_frcp_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_frsq_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_flog_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_flogcn_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fexp_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fred_variant(pco_instr *instr)
{
   return PCO_MAIN_FRED;
}

static inline
unsigned pco_fsinc_variant(pco_instr *instr)
{
   return PCO_MAIN_SNGL;
}

static inline
unsigned pco_mbyp_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fdsx_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fdsxf_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fdsy_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_fdsyf_variant(pco_instr *instr)
{
   if (instr->src[0].neg == false && instr->src[0].abs == false)
      return PCO_MAIN_SNGL;

   return PCO_MAIN_SNGL_EXT;
}

static inline
unsigned pco_pck_variant(pco_instr *instr)
{
   return PCO_MAIN_PCK;
}

static inline
unsigned pco_pck_prog_variant(pco_instr *instr)
{
   return PCO_MAIN_PCK;
}

static inline
unsigned pco_unpck_variant(pco_instr *instr)
{
   return PCO_MAIN_UPCK;
}

static inline
unsigned pco_tst_variant(pco_instr *instr)
{
   if (pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN) <= PCO_TST_OP_MAIN_NOTEQUAL && pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN) == PCO_TST_TYPE_MAIN_F32 && pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END) == false && pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN) == PCO_TST_TYPE_MAIN_F32 && instr->src[0].elem == 0 && instr->src[1].elem == 0)
      return PCO_MAIN_TST;

   return PCO_MAIN_TST_EXT;
}

static inline
unsigned pco_movc_variant(pco_instr *instr)
{
   if (instr->dest[0].elem == 0b1111 && pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END) == false)
      return PCO_MAIN_MOVC;

   return PCO_MAIN_MOVC_EXT;
}

static inline
unsigned pco_movwm_variant(pco_instr *instr)
{
   return PCO_MAIN_MOVC_EXT;
}

static inline
unsigned pco_add64_32_variant(pco_instr *instr)
{
   if (pco_ref_is_null(instr->src[3]) && instr->src[0].neg == false && instr->src[0].abs == false && instr->src[2].abs == false)
      return PCO_MAIN_INT32_64;

   return PCO_MAIN_INT32_64_EXT;
}

static inline
unsigned pco_imadd64_variant(pco_instr *instr)
{
   if (pco_ref_is_null(instr->src[4]) && instr->src[0].neg == false && instr->src[0].abs == false && instr->src[1].neg == false && instr->src[1].abs == false && instr->src[2].abs == false)
      return PCO_MAIN_INT32_64;

   return PCO_MAIN_INT32_64_EXT;
}

static inline
unsigned pco_imadd32_variant(pco_instr *instr)
{
   if (pco_ref_is_null(instr->src[3]) && instr->src[0].neg == false && instr->src[0].abs == false && instr->src[1].neg == false && instr->src[1].abs == false && instr->src[2].abs == false)
      return PCO_MAIN_INT32_64;

   return PCO_MAIN_INT32_64_EXT;
}

static inline
unsigned pco_uvsw_write_variant(pco_instr *instr)
{
   return PCO_BACKEND_UVSW_WRITE_IMM;
}

static inline
unsigned pco_uvsw_emit_variant(pco_instr *instr)
{
   return PCO_BACKEND_UVSW_EMIT;
}

static inline
unsigned pco_uvsw_endtask_variant(pco_instr *instr)
{
   return PCO_BACKEND_UVSW_ENDTASK;
}

static inline
unsigned pco_uvsw_emit_endtask_variant(pco_instr *instr)
{
   return PCO_BACKEND_UVSW_EMIT_ENDTASK;
}

static inline
unsigned pco_uvsw_write_emit_endtask_variant(pco_instr *instr)
{
   return PCO_BACKEND_UVSW_WRITE_EMIT_ENDTASK_IMM;
}

static inline
unsigned pco_fitr_variant(pco_instr *instr)
{
   return PCO_BACKEND_FITR;
}

static inline
unsigned pco_fitrp_variant(pco_instr *instr)
{
   return PCO_BACKEND_FITR;
}

static inline
unsigned pco_ld_variant(pco_instr *instr)
{
   return PCO_BACKEND_LD_IMMBL;
}

static inline
unsigned pco_ld_regbl_variant(pco_instr *instr)
{
   return PCO_BACKEND_LD_REGBL;
}

static inline
unsigned pco_st_variant(pco_instr *instr)
{
   return PCO_BACKEND_ST_IMMBL;
}

static inline
unsigned pco_st_regbl_variant(pco_instr *instr)
{
   return PCO_BACKEND_ST_REGBL;
}

static inline
unsigned pco_st_tiled_variant(pco_instr *instr)
{
   return PCO_BACKEND_ST_IMMBL_TILED;
}

static inline
unsigned pco_idf_variant(pco_instr *instr)
{
   return PCO_BACKEND_IDF;
}

static inline
unsigned pco_atomic_variant(pco_instr *instr)
{
   return PCO_BACKEND_ATOMIC;
}

static inline
unsigned pco_smp_variant(pco_instr *instr)
{
   if (pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER) == false && pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY) == false && pco_instr_get_mod(instr, PCO_OP_MOD_F16) == false && pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP) == PCO_SCHEDSWAP_NONE && pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD) == PCO_CACHEMODE_LD_NORMAL && pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD) == false && pco_instr_get_mod(instr, PCO_OP_MOD_PROJ) == false && pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE) == PCO_SB_MODE_NONE && pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS) == false && pco_instr_get_mod(instr, PCO_OP_MOD_SNO) == false && pco_instr_get_mod(instr, PCO_OP_MOD_SOO) == false && pco_instr_get_mod(instr, PCO_OP_MOD_TAO) == false)
      return PCO_BACKEND_SMP_BRIEF;
   else if (pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER) == false && pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY) == false && pco_instr_get_mod(instr, PCO_OP_MOD_F16) == false && pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP) == PCO_SCHEDSWAP_NONE && pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD) == PCO_CACHEMODE_LD_NORMAL)
      return PCO_BACKEND_SMP_EXTA;
   else if (pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER) == false && pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY) == false)
      return PCO_BACKEND_SMP_EXTAB;

   return PCO_BACKEND_SMP_EXTABC;
}

static inline
unsigned pco_smp_wrt_variant(pco_instr *instr)
{
   if (pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER) == false && pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY) == false)
      return PCO_BACKEND_SMP_EXTAB;

   return PCO_BACKEND_SMP_EXTABC;
}

static inline
unsigned pco_alphatst_variant(pco_instr *instr)
{
   return PCO_BACKEND_VISTEST_ATST;
}

static inline
unsigned pco_alphaf_variant(pco_instr *instr)
{
   return PCO_BACKEND_VISTEST_ATST;
}

static inline
unsigned pco_depthf_variant(pco_instr *instr)
{
   return PCO_BACKEND_VISTEST_DEPTHF;
}

static inline
unsigned pco_savmsk_variant(pco_instr *instr)
{
   return PCO_BACKEND_SAVMSK;
}

static inline
unsigned pco_emitpix_variant(pco_instr *instr)
{
   return PCO_BACKEND_EMITPIX;
}

static inline
unsigned pco_setl_variant(pco_instr *instr)
{
   return PCO_BACKEND_SETL;
}

static inline
unsigned pco_bbyp0bm_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_bbyp0bm_imm32_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_IMM32;
}

static inline
unsigned pco_bbyp0s1_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_msk_bbyp0s1_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_msk_lsl_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_cbs_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_ftb_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_rev_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_shuffle_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE0_SRC;
}

static inline
unsigned pco_logical_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE1;
}

static inline
unsigned pco_shift_variant(pco_instr *instr)
{
   return PCO_BITWISE_PHASE2;
}

static inline
unsigned pco_wop_variant(pco_instr *instr)
{
   return PCO_CTRL_WOP;
}

static inline
unsigned pco_wdf_variant(pco_instr *instr)
{
   return PCO_CTRL_WDF;
}

static inline
unsigned pco_nop_variant(pco_instr *instr)
{
   return PCO_CTRL_NOP;
}

static inline
unsigned pco_ditr_variant(pco_instr *instr)
{
   return PCO_CTRL_DITR;
}

static inline
unsigned pco_ditrp_variant(pco_instr *instr)
{
   return PCO_CTRL_DITR;
}

static inline
unsigned pco_cndst_variant(pco_instr *instr)
{
   return PCO_CTRL_CND;
}

static inline
unsigned pco_cndef_variant(pco_instr *instr)
{
   return PCO_CTRL_CND;
}

static inline
unsigned pco_cndsm_variant(pco_instr *instr)
{
   return PCO_CTRL_CND;
}

static inline
unsigned pco_cndlt_variant(pco_instr *instr)
{
   return PCO_CTRL_CND;
}

static inline
unsigned pco_cndend_variant(pco_instr *instr)
{
   return PCO_CTRL_CND;
}

static inline
unsigned pco_br_variant(pco_instr *instr)
{
   return PCO_CTRL_BRANCH;
}

static inline
unsigned pco_br_next_variant(pco_instr *instr)
{
   return PCO_CTRL_BRANCH;
}

static inline
unsigned pco_br_skip_next_variant(pco_instr *instr)
{
   return PCO_CTRL_BRANCH;
}

static inline
unsigned pco_mutex_variant(pco_instr *instr)
{
   return PCO_CTRL_MUTEX;
}

static inline
unsigned pco_savl_variant(pco_instr *instr)
{
   return PCO_CTRL_SAVL;
}

/* Instruction group mappings. */
static inline
void pco_fadd_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fadd_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fmul_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fmul_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fmad_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fmad_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_frcp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_frcp_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_frsq_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_frsq_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_flog_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_flog_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_flogcn_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_flogcn_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fexp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fexp_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fred_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = !pco_ref_is_null(instr->dest[1]);
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[3], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[1];
   igrp->instrs[PCO_OP_PHASE_0]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fred_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fsinc_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fsinc_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_mbyp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fdsx_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fdsx_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fdsxf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fdsxf_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fdsy_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fdsy_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fdsyf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fdsyf_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_pck_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_2_PCK] = instr;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_2_PCK]->src[0];
   igrp->instrs[PCO_OP_PHASE_2_PCK]->src[0] = pco_ref_io(PCO_IO_IS3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_PCK]->src[0], &igrp->srcs.s[2], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S2);
   igrp->iss.is[3] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT2);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0] = pco_ref_io(PCO_IO_FT2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_pck_prog_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_PCK] = pco_pck_prog_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_IS3), instr->src[1], .roundzero = pco_instr_get_mod(instr, PCO_OP_MOD_ROUNDZERO), .scale = pco_instr_get_mod(instr, PCO_OP_MOD_SCALE));
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_2_PCK]->src[1];
   igrp->instrs[PCO_OP_PHASE_2_PCK]->src[1] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_PCK]->src[1], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S3);
   igrp->iss.is[3] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT2);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0] = pco_ref_io(PCO_IO_FT2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_PCK]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_prog_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_unpck_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_unpck_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_movwm_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movwm_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT0), instr->src[1], .phase2end = pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END));
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1] = pco_ref_io(PCO_IO_IS4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1], &igrp->srcs.s[1], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movwm_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_movs1_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movwm_(instr->parent_func, instr->dest[0], instr->src[0], pco_ref_io(PCO_IO_IS4), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->src[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->src[0] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->src[0], &igrp->srcs.s[1], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movwm_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_add64_32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = !pco_ref_is_null(instr->dest[1]);
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[1];
   igrp->instrs[PCO_OP_PHASE_0]->dest[1] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_add64_32_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_imadd64_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = !pco_ref_is_null(instr->dest[1]);
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_0]->src[3];
   igrp->instrs[PCO_OP_PHASE_0]->src[3] = pco_ref_io(PCO_IO_IS0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[3], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S3);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[1];
   igrp->instrs[PCO_OP_PHASE_0]->dest[1] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd64_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_imadd32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd32_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_scmp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN), .tst_type_main = PCO_TST_TYPE_MAIN_F32);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_PCK] = pco_pck_(instr->parent_func, pco_ref_io(PCO_IO_FT2), pco_ref_null(), .pck_fmt = PCO_PCK_FMT_ONE);
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = pco_zero;
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_bcmp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN), .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN));
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_PCK] = pco_pck_(instr->parent_func, pco_ref_io(PCO_IO_FT2), pco_ref_null(), .pck_fmt = PCO_PCK_FMT_ZERO);
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FTE), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = pco_true;
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT2);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_bcsel_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[2]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), instr->src[0], pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_ZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_2_TST]->src[0];
   igrp->instrs[PCO_OP_PHASE_2_TST]->src[0] = pco_ref_io(PCO_IO_IS1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_TST]->src[0], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_csel_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[2]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), instr->src[0], pco_ref_null(), .tst_op_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN), .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_2_TST]->src[0];
   igrp->instrs[PCO_OP_PHASE_2_TST]->src[0] = pco_ref_io(PCO_IO_IS1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_TST]->src[0], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_psel_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd64_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FTE), pco_zero, pco_zero, pco_zero, pco_ref_io(PCO_IO_IS0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_ZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), instr->src[2], pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1], &igrp->srcs.s[4], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S4);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd64_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_psel_trig_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd64_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FTE), pco_zero, pco_zero, pco_zero, pco_ref_io(PCO_IO_IS0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_fmul_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1], instr->src[2]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_ZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FTE), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_1]->src[1];
   igrp->instrs[PCO_OP_PHASE_1]->src[1] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[1], &igrp->srcs.s[4], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S3);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd64_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_fmul_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fsign_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_fmul_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0], pco_finf, .sat = true);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), pco_fnegone);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_GEZERO, .tst_type_main = PCO_TST_TYPE_MAIN_S32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S0);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fmul_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_isign_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd64_(instr->parent_func, pco_ref_null(), pco_ref_io(PCO_IO_FTE), instr->src[0], pco_one, pco_zero, pco_ref_io(PCO_IO_IS0), pco_ref_null(), .s = true);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), pco_one);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_GZERO, .tst_type_main = PCO_TST_TYPE_MAIN_S32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S2);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd64_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fceil_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_fadd_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_flr(instr->src[0]), pco_fone);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_fadd_(instr->parent_func, pco_ref_io(PCO_IO_FT1), pco_ref_flr(instr->src[0]), pco_nzero);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_EQUAL, .tst_type_main = PCO_TST_TYPE_MAIN_F32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_1]->src[1];
   igrp->instrs[PCO_OP_PHASE_1]->src[1] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[1], &igrp->srcs.s[4], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S0);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_fadd_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_fadd_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_min_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_LESS, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_max_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_GREATER, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_iadd32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd32_(instr->parent_func, instr->dest[0], instr->src[0], pco_one, instr->src[1], instr->src[2], .s = pco_instr_get_mod(instr, PCO_OP_MOD_S));
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd32_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_imul32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd32_(instr->parent_func, instr->dest[0], instr->src[0], instr->src[1], pco_zero, instr->src[2], .s = pco_instr_get_mod(instr, PCO_OP_MOD_S));
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd32_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uadd_carry_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = !pco_ref_is_null(instr->dest[1]);
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_add64_32_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FTE), instr->src[0], pco_zero, instr->src[1], pco_ref_null(), .s = false);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), pco_one);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_GZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_PCK] = pco_pck_(instr->parent_func, pco_ref_io(PCO_IO_FT2), pco_ref_null(), .pck_fmt = PCO_PCK_FMT_ZERO);
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], instr->dest[1], pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS5));
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S3);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT2);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_add64_32_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uadd_sat_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_add64_32_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FTE), instr->src[0], pco_zero, instr->src[1], pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[2]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_GZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S3);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FTE);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_add64_32_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_tstz_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), instr->dest[1], pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_ZERO, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN));
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_PCK] = pco_pck_(instr->parent_func, pco_ref_io(PCO_IO_FT2), pco_ref_null(), .pck_fmt = PCO_PCK_FMT_ZERO);
   igrp->instrs[PCO_OP_PHASE_2_PCK]->phase = PCO_OP_PHASE_2_PCK;
   igrp->instrs[PCO_OP_PHASE_2_PCK]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[0], pco_ref_null(), pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FTE), pco_ref_io(PCO_IO_IS4), pco_ref_null(), pco_ref_null());
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = pco_true;

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT2);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_PCK].main = pco_pck_variant(igrp->instrs[PCO_OP_PHASE_2_PCK]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_st32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_BACKEND] = pco_st_(instr->parent_func, instr->src[0], pco_ref_imm8(PCO_DSIZE_32BIT), instr->src[1], instr->src[2], instr->src[3], instr->src[4], .mcu_cache_mode_st = pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST), .idf = pco_instr_get_mod(instr, PCO_OP_MOD_IDF));
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_st_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_st32_regbl_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_BACKEND] = pco_st_regbl_(instr->parent_func, instr->src[0], pco_ref_imm8(PCO_DSIZE_32BIT), instr->src[1], instr->src[2], instr->src[3], instr->src[4], .mcu_cache_mode_st = pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST), .idf = pco_instr_get_mod(instr, PCO_OP_MOD_IDF));
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_st_regbl_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_st_tiled_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[5];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[5] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[5], &igrp->srcs.s[4], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_st_tiled_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_idf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_idf_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_iadd32_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd32_(instr->parent_func, instr->dest[1], instr->src[0], pco_one, instr->src[1], instr->src[2], .s = pco_instr_get_mod(instr, PCO_OP_MOD_S));
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, instr->dest[0], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd32_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_xchg_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, instr->dest[1], instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, instr->dest[0], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT0);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cmpxchg_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_EQUAL, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[1], instr->dest[0], pco_ref_io(PCO_IO_FTT), instr->src[2], pco_ref_io(PCO_IO_IS4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS5));
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1] = pco_ref_io(PCO_IO_FTE);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S1);
   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_min_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_LESS, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[1], instr->dest[0], pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS5));
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_max_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, pco_ref_io(PCO_IO_FT1), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_ref_null(), pco_ref_io(PCO_IO_IS1), pco_ref_io(PCO_IO_IS2), .tst_op_main = PCO_TST_OP_MAIN_GREATER, .tst_type_main = pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN), .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_MOV] = pco_movc_(instr->parent_func, instr->dest[1], instr->dest[0], pco_ref_io(PCO_IO_FTT), pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_IS4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_IS5));
   igrp->instrs[PCO_OP_PHASE_2_MOV]->phase = PCO_OP_PHASE_2_MOV;
   igrp->instrs[PCO_OP_PHASE_2_MOV]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[2] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT1);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1];
   igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2_MOV]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);
   igrp->variant.instr[PCO_OP_PHASE_2_MOV].main = pco_movc_variant(igrp->instrs[PCO_OP_PHASE_2_MOV]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_logical_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = true;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_bbyp0s1_(instr->parent_func, pco_ref_io(PCO_IO_FT2), instr->dest[0], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, instr->dest[1], pco_ref_null(), pco_ref_io(PCO_IO_FT2), pco_ref_null(), instr->src[1], .logiop = pco_instr_get_mod(instr, PCO_OP_MOD_LOGIOP));
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[1];
   igrp->instrs[PCO_OP_PHASE_0]->dest[1] = pco_ref_io(PCO_IO_FT3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_bbyp0s1_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_flush_p0_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P2;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_imadd32_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_zero, pco_zero, pco_zero, pco_p0, .s = false);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2_TST] = pco_tst_(instr->parent_func, pco_ref_io(PCO_IO_FTT), pco_p0, pco_ref_io(PCO_IO_IS1), pco_ref_null(), .tst_op_main = PCO_TST_OP_MAIN_GZERO, .tst_type_main = PCO_TST_TYPE_MAIN_U32, .phase2end = true);
   igrp->instrs[PCO_OP_PHASE_2_TST]->phase = PCO_OP_PHASE_2_TST;
   igrp->instrs[PCO_OP_PHASE_2_TST]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);

   igrp->iss.is[1] = pco_ref_io(PCO_IO_FT0);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_imadd32_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_2_TST].main = pco_tst_variant(igrp->instrs[PCO_OP_PHASE_2_TST]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_mbyp2_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_P0_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   igrp->instrs[PCO_OP_PHASE_0] = pco_mbyp_(instr->parent_func, instr->dest[0], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_mbyp_(instr->parent_func, instr->dest[1], instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[0];
   igrp->instrs[PCO_OP_PHASE_1]->src[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[0], &igrp->srcs.s[3], true);

   igrp->iss.is[4] = pco_ref_io(PCO_IO_FT0);
   igrp->iss.is[5] = pco_ref_io(PCO_IO_FT1);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].main = pco_mbyp_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uvsw_write_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = false;
   igrp->hdr.atom = false;
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_uvsw_write_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uvsw_emit_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = false;
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_uvsw_emit_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uvsw_endtask_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_uvsw_endtask_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uvsw_emit_endtask_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_uvsw_emit_endtask_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_uvsw_write_emit_endtask_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_uvsw_write_emit_endtask_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fitr_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_fitr_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_fitrp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_fitrp_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ld_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_ld_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ld_regbl_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_ld_regbl_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_atomic_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_atomic_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_smp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->srcs.s[4], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_smp_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_smp_wrt_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[4], &igrp->srcs.s[3], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_smp_wrt_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_alphatst_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3], &igrp->srcs.s[2], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_alphatst_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_alphaf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[2], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[3], &igrp->srcs.s[2], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_alphaf_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_depthf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[0], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S0);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_depthf_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_savmsk_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = !pco_ref_is_null(instr->dest[1]);
   igrp->hdr.w0p = !pco_ref_is_null(instr->dest[0]);
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[0], &igrp->dests.w[0], true);
   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[1] = pco_ref_io(PCO_IO_W1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->dest[1], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_savmsk_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_emitpix_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[1], &igrp->srcs.s[2], true);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_emitpix_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_setl_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.oporg = PCO_OPORG_BE;
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = false;
   igrp->hdr.atom = false;
   igrp->hdr.rpt = 1;
   igrp->hdr.alutype = PCO_ALUTYPE_MAIN;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_BACKEND] = instr;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->phase = PCO_OP_PHASE_BACKEND;
   igrp->instrs[PCO_OP_PHASE_BACKEND]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0];
   igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_BACKEND]->src[0], &igrp->srcs.s[2], true);

   igrp->iss.is[0] = pco_ref_io(PCO_IO_S2);
   igrp->iss.is[4] = pco_ref_io(PCO_IO_FTE);

   igrp->variant.instr[PCO_OP_PHASE_BACKEND].main = pco_setl_variant(igrp->instrs[PCO_OP_PHASE_BACKEND]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_movi32_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_bbyp0bm_imm32_(instr->parent_func, pco_ref_io(PCO_IO_FT0), instr->dest[0], pco_ref_io(PCO_IO_S0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_0]->dest[1];
   igrp->instrs[PCO_OP_PHASE_0]->dest[1] = pco_ref_io(PCO_IO_FT1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_bbyp0bm_imm32_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cbs_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);

   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_cbs_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ftb_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);

   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_ftb_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_rev_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);

   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_rev_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_shuffle_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   pco_instr_set_mod(instr, PCO_OP_MOD_OLCHK, 0);
   igrp->hdr.w1p = true;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   pco_instr_set_mod(instr, PCO_OP_MOD_ATOM, 0);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   pco_instr_set_mod(instr, PCO_OP_MOD_RPT, 1);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_0] = instr;
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);

   igrp->dests.w[1] = igrp->instrs[PCO_OP_PHASE_0]->dest[0];
   igrp->instrs[PCO_OP_PHASE_0]->dest[0] = pco_ref_io(PCO_IO_FT2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->dest[0], &igrp->dests.w[1], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_shuffle_variant(igrp->instrs[PCO_OP_PHASE_0]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_logical_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_bbyp0s1_(instr->parent_func, pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_FT3), instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, instr->dest[0], instr->src[0], pco_ref_io(PCO_IO_FT2), instr->src[2], instr->src[3], .logiop = pco_instr_get_mod(instr, PCO_OP_MOD_LOGIOP));
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_bbyp0s1_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_shift_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_bbyp0bm_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_S0), instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2] = pco_shift_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT4), instr->src[1], instr->src[2], .shiftop = pco_instr_get_mod(instr, PCO_OP_MOD_SHIFTOP));
   igrp->instrs[PCO_OP_PHASE_2]->phase = PCO_OP_PHASE_2;
   igrp->instrs[PCO_OP_PHASE_2]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_2]->src[1];
   igrp->instrs[PCO_OP_PHASE_2]->src[1] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->src[1], &igrp->srcs.s[4], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2]->dest[0] = pco_ref_io(PCO_IO_FT5);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_bbyp0bm_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_2].bitwise = pco_shift_variant(igrp->instrs[PCO_OP_PHASE_2]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_copysign_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_msk_bbyp0s1_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), pco_31, pco_zero, instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_FT1_INVERT), instr->src[1], .logiop = PCO_LOGIOP_OR);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_msk_bbyp0s1_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ibfe_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1 | PCO_OPCNT_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_msk_bbyp0s1_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), instr->src[2], instr->src[1], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, pco_ref_io(PCO_IO_FT4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_FT1_INVERT), pco_zero, .logiop = PCO_LOGIOP_OR);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2] = pco_shift_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT4), instr->src[1], pco_ref_io(PCO_IO_FT0), .shiftop = PCO_SHIFTOP_ASR_MTB);
   igrp->instrs[PCO_OP_PHASE_2]->phase = PCO_OP_PHASE_2;
   igrp->instrs[PCO_OP_PHASE_2]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_2]->src[1];
   igrp->instrs[PCO_OP_PHASE_2]->src[1] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->src[1], &igrp->srcs.s[4], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2]->dest[0] = pco_ref_io(PCO_IO_FT5);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_msk_bbyp0s1_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2].bitwise = pco_shift_variant(igrp->instrs[PCO_OP_PHASE_2]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ubfe_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1 | PCO_OPCNT_P2;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_msk_bbyp0s1_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), instr->src[2], instr->src[1], instr->src[0]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, pco_ref_io(PCO_IO_FT4), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_FT1_INVERT), pco_zero, .logiop = PCO_LOGIOP_OR);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_2] = pco_shift_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT4), instr->src[1], pco_ref_null(), .shiftop = PCO_SHIFTOP_SHR);
   igrp->instrs[PCO_OP_PHASE_2]->phase = PCO_OP_PHASE_2;
   igrp->instrs[PCO_OP_PHASE_2]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);
   igrp->srcs.s[4] = igrp->instrs[PCO_OP_PHASE_2]->src[1];
   igrp->instrs[PCO_OP_PHASE_2]->src[1] = pco_ref_io(PCO_IO_S4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->src[1], &igrp->srcs.s[4], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_2]->dest[0];
   igrp->instrs[PCO_OP_PHASE_2]->dest[0] = pco_ref_io(PCO_IO_FT5);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_2]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_msk_bbyp0s1_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);
   igrp->variant.instr[PCO_OP_PHASE_2].bitwise = pco_shift_variant(igrp->instrs[PCO_OP_PHASE_2]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_bfi_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.opcnt = PCO_OPCNT_P0 | PCO_OPCNT_P1;
   igrp->hdr.olchk = pco_instr_get_mod(instr, PCO_OP_MOD_OLCHK);
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   igrp->hdr.end = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   igrp->hdr.atom = pco_instr_get_mod(instr, PCO_OP_MOD_ATOM);
   igrp->hdr.rpt = pco_instr_get_mod(instr, PCO_OP_MOD_RPT);
   igrp->hdr.alutype = PCO_ALUTYPE_BITWISE;

   igrp->instrs[PCO_OP_PHASE_0] = pco_msk_lsl_(instr->parent_func, pco_ref_io(PCO_IO_FT0), pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), instr->src[3], instr->src[2], instr->src[1]);
   igrp->instrs[PCO_OP_PHASE_0]->phase = PCO_OP_PHASE_0;
   igrp->instrs[PCO_OP_PHASE_0]->parent_igrp = igrp;
   igrp->instrs[PCO_OP_PHASE_1] = pco_logical_(instr->parent_func, instr->dest[0], pco_ref_io(PCO_IO_FT1), pco_ref_io(PCO_IO_FT2), pco_ref_io(PCO_IO_FT1_INVERT), instr->src[0], .logiop = PCO_LOGIOP_OR);
   igrp->instrs[PCO_OP_PHASE_1]->phase = PCO_OP_PHASE_1;
   igrp->instrs[PCO_OP_PHASE_1]->parent_igrp = igrp;
   pco_instr_delete(instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_0]->src[0];
   igrp->instrs[PCO_OP_PHASE_0]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[1] = igrp->instrs[PCO_OP_PHASE_0]->src[1];
   igrp->instrs[PCO_OP_PHASE_0]->src[1] = pco_ref_io(PCO_IO_S1);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[1], &igrp->srcs.s[1], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_0]->src[2];
   igrp->instrs[PCO_OP_PHASE_0]->src[2] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_0]->src[2], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = igrp->instrs[PCO_OP_PHASE_1]->src[3];
   igrp->instrs[PCO_OP_PHASE_1]->src[3] = pco_ref_io(PCO_IO_S3);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->src[3], &igrp->srcs.s[3], true);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_1]->dest[0];
   igrp->instrs[PCO_OP_PHASE_1]->dest[0] = pco_ref_io(PCO_IO_FT4);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_1]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_0].bitwise = pco_msk_lsl_variant(igrp->instrs[PCO_OP_PHASE_0]);
   igrp->variant.instr[PCO_OP_PHASE_1].bitwise = pco_logical_variant(igrp->instrs[PCO_OP_PHASE_1]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_wop_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.miscctl = 0;
   igrp->hdr.ctrlop = PCO_CTRLOP_WOP;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_wop_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_wdf_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.miscctl = pco_ref_get_drc(instr->src[0]);
   igrp->hdr.ctrlop = PCO_CTRLOP_WDF;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_wdf_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_nop_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = pco_instr_get_mod(instr, PCO_OP_MOD_END);
   pco_instr_set_mod(instr, PCO_OP_MOD_END, 0);
   igrp->hdr.ctrlop = PCO_CTRLOP_NOP;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_nop_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ditr_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_DITR;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_ditr_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_ditrp_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_DITR;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_ditrp_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cndst_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_CND;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = pco_zero;

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_cndst_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cndef_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_CND;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = pco_zero;

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_cndef_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cndsm_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_CND;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[2] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[1] = pco_ref_io(PCO_IO_S2);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[1], &igrp->srcs.s[2], true);
   igrp->srcs.s[3] = pco_zero;

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_cndsm_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cndlt_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_CND;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = pco_zero;

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_cndlt_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_cndend_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_CND;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->srcs.s[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->src[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->src[0] = pco_ref_io(PCO_IO_S0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->src[0], &igrp->srcs.s[0], true);
   igrp->srcs.s[3] = pco_zero;

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[1], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_cndend_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_br_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_B;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_br_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_br_next_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_B;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_br_next_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_br_skip_next_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_B;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_br_skip_next_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_mutex_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = false;
   igrp->hdr.cc = PCO_CC_E1_ZX;
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_MUTEX;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_mutex_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_savl_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   igrp->hdr.olchk = false;
   igrp->hdr.w1p = false;
   igrp->hdr.w0p = true;
   igrp->hdr.cc = pco_map_exec_cnd_to_cc(pco_instr_get_mod(instr, PCO_OP_MOD_EXEC_CND));
   pco_instr_set_mod(instr, PCO_OP_MOD_EXEC_CND, 0);
   igrp->hdr.miscctl = false;
   igrp->hdr.ctrlop = PCO_CTRLOP_SAVL;
   igrp->hdr.alutype = PCO_ALUTYPE_CONTROL;

   list_del(&instr->link);
   igrp->instrs[PCO_OP_PHASE_CTRL] = instr;
   igrp->instrs[PCO_OP_PHASE_CTRL]->phase = PCO_OP_PHASE_CTRL;
   igrp->instrs[PCO_OP_PHASE_CTRL]->parent_igrp = igrp;
   ralloc_steal(igrp, instr);

   igrp->dests.w[0] = igrp->instrs[PCO_OP_PHASE_CTRL]->dest[0];
   igrp->instrs[PCO_OP_PHASE_CTRL]->dest[0] = pco_ref_io(PCO_IO_W0);
   pco_ref_xfer_mods(&igrp->instrs[PCO_OP_PHASE_CTRL]->dest[0], &igrp->dests.w[0], true);

   igrp->variant.instr[PCO_OP_PHASE_CTRL].control = pco_savl_variant(igrp->instrs[PCO_OP_PHASE_CTRL]);

   igrp->variant.hdr = pco_igrp_hdr_variant(igrp);
   igrp->variant.lower_src = pco_igrp_src_variant(igrp, false);
   igrp->variant.upper_src = pco_igrp_src_variant(igrp, true);
   igrp->variant.iss = pco_igrp_iss_variant(igrp);
   igrp->variant.dest = pco_igrp_dest_variant(igrp);
}

static inline
void pco_map_igrp(pco_igrp *igrp, pco_instr *instr)
{
   switch (instr->op) {
   case PCO_OP_FADD:
      return pco_fadd_map_igrp(igrp, instr);

   case PCO_OP_FMUL:
      return pco_fmul_map_igrp(igrp, instr);

   case PCO_OP_FMAD:
      return pco_fmad_map_igrp(igrp, instr);

   case PCO_OP_FRCP:
      return pco_frcp_map_igrp(igrp, instr);

   case PCO_OP_FRSQ:
      return pco_frsq_map_igrp(igrp, instr);

   case PCO_OP_FLOG:
      return pco_flog_map_igrp(igrp, instr);

   case PCO_OP_FLOGCN:
      return pco_flogcn_map_igrp(igrp, instr);

   case PCO_OP_FEXP:
      return pco_fexp_map_igrp(igrp, instr);

   case PCO_OP_FRED:
      return pco_fred_map_igrp(igrp, instr);

   case PCO_OP_FSINC:
      return pco_fsinc_map_igrp(igrp, instr);

   case PCO_OP_MBYP:
      return pco_mbyp_map_igrp(igrp, instr);

   case PCO_OP_FDSX:
      return pco_fdsx_map_igrp(igrp, instr);

   case PCO_OP_FDSXF:
      return pco_fdsxf_map_igrp(igrp, instr);

   case PCO_OP_FDSY:
      return pco_fdsy_map_igrp(igrp, instr);

   case PCO_OP_FDSYF:
      return pco_fdsyf_map_igrp(igrp, instr);

   case PCO_OP_PCK:
      return pco_pck_map_igrp(igrp, instr);

   case PCO_OP_PCK_PROG:
      return pco_pck_prog_map_igrp(igrp, instr);

   case PCO_OP_UNPCK:
      return pco_unpck_map_igrp(igrp, instr);

   case PCO_OP_MOVWM:
      return pco_movwm_map_igrp(igrp, instr);

   case PCO_OP_MOVS1:
      return pco_movs1_map_igrp(igrp, instr);

   case PCO_OP_ADD64_32:
      return pco_add64_32_map_igrp(igrp, instr);

   case PCO_OP_IMADD64:
      return pco_imadd64_map_igrp(igrp, instr);

   case PCO_OP_IMADD32:
      return pco_imadd32_map_igrp(igrp, instr);

   case PCO_OP_SCMP:
      return pco_scmp_map_igrp(igrp, instr);

   case PCO_OP_BCMP:
      return pco_bcmp_map_igrp(igrp, instr);

   case PCO_OP_BCSEL:
      return pco_bcsel_map_igrp(igrp, instr);

   case PCO_OP_CSEL:
      return pco_csel_map_igrp(igrp, instr);

   case PCO_OP_PSEL:
      return pco_psel_map_igrp(igrp, instr);

   case PCO_OP_PSEL_TRIG:
      return pco_psel_trig_map_igrp(igrp, instr);

   case PCO_OP_FSIGN:
      return pco_fsign_map_igrp(igrp, instr);

   case PCO_OP_ISIGN:
      return pco_isign_map_igrp(igrp, instr);

   case PCO_OP_FCEIL:
      return pco_fceil_map_igrp(igrp, instr);

   case PCO_OP_MIN:
      return pco_min_map_igrp(igrp, instr);

   case PCO_OP_MAX:
      return pco_max_map_igrp(igrp, instr);

   case PCO_OP_IADD32:
      return pco_iadd32_map_igrp(igrp, instr);

   case PCO_OP_IMUL32:
      return pco_imul32_map_igrp(igrp, instr);

   case PCO_OP_UADD_CARRY:
      return pco_uadd_carry_map_igrp(igrp, instr);

   case PCO_OP_UADD_SAT:
      return pco_uadd_sat_map_igrp(igrp, instr);

   case PCO_OP_TSTZ:
      return pco_tstz_map_igrp(igrp, instr);

   case PCO_OP_ST32:
      return pco_st32_map_igrp(igrp, instr);

   case PCO_OP_ST32_REGBL:
      return pco_st32_regbl_map_igrp(igrp, instr);

   case PCO_OP_ST_TILED:
      return pco_st_tiled_map_igrp(igrp, instr);

   case PCO_OP_IDF:
      return pco_idf_map_igrp(igrp, instr);

   case PCO_OP_IADD32_ATOMIC:
      return pco_iadd32_atomic_map_igrp(igrp, instr);

   case PCO_OP_XCHG_ATOMIC:
      return pco_xchg_atomic_map_igrp(igrp, instr);

   case PCO_OP_CMPXCHG_ATOMIC:
      return pco_cmpxchg_atomic_map_igrp(igrp, instr);

   case PCO_OP_MIN_ATOMIC:
      return pco_min_atomic_map_igrp(igrp, instr);

   case PCO_OP_MAX_ATOMIC:
      return pco_max_atomic_map_igrp(igrp, instr);

   case PCO_OP_LOGICAL_ATOMIC:
      return pco_logical_atomic_map_igrp(igrp, instr);

   case PCO_OP_FLUSH_P0:
      return pco_flush_p0_map_igrp(igrp, instr);

   case PCO_OP_MBYP2:
      return pco_mbyp2_map_igrp(igrp, instr);

   case PCO_OP_UVSW_WRITE:
      return pco_uvsw_write_map_igrp(igrp, instr);

   case PCO_OP_UVSW_EMIT:
      return pco_uvsw_emit_map_igrp(igrp, instr);

   case PCO_OP_UVSW_ENDTASK:
      return pco_uvsw_endtask_map_igrp(igrp, instr);

   case PCO_OP_UVSW_EMIT_ENDTASK:
      return pco_uvsw_emit_endtask_map_igrp(igrp, instr);

   case PCO_OP_UVSW_WRITE_EMIT_ENDTASK:
      return pco_uvsw_write_emit_endtask_map_igrp(igrp, instr);

   case PCO_OP_FITR:
      return pco_fitr_map_igrp(igrp, instr);

   case PCO_OP_FITRP:
      return pco_fitrp_map_igrp(igrp, instr);

   case PCO_OP_LD:
      return pco_ld_map_igrp(igrp, instr);

   case PCO_OP_LD_REGBL:
      return pco_ld_regbl_map_igrp(igrp, instr);

   case PCO_OP_ATOMIC:
      return pco_atomic_map_igrp(igrp, instr);

   case PCO_OP_SMP:
      return pco_smp_map_igrp(igrp, instr);

   case PCO_OP_SMP_WRT:
      return pco_smp_wrt_map_igrp(igrp, instr);

   case PCO_OP_ALPHATST:
      return pco_alphatst_map_igrp(igrp, instr);

   case PCO_OP_ALPHAF:
      return pco_alphaf_map_igrp(igrp, instr);

   case PCO_OP_DEPTHF:
      return pco_depthf_map_igrp(igrp, instr);

   case PCO_OP_SAVMSK:
      return pco_savmsk_map_igrp(igrp, instr);

   case PCO_OP_EMITPIX:
      return pco_emitpix_map_igrp(igrp, instr);

   case PCO_OP_SETL:
      return pco_setl_map_igrp(igrp, instr);

   case PCO_OP_MOVI32:
      return pco_movi32_map_igrp(igrp, instr);

   case PCO_OP_CBS:
      return pco_cbs_map_igrp(igrp, instr);

   case PCO_OP_FTB:
      return pco_ftb_map_igrp(igrp, instr);

   case PCO_OP_REV:
      return pco_rev_map_igrp(igrp, instr);

   case PCO_OP_SHUFFLE:
      return pco_shuffle_map_igrp(igrp, instr);

   case PCO_OP_LOGICAL:
      return pco_logical_map_igrp(igrp, instr);

   case PCO_OP_SHIFT:
      return pco_shift_map_igrp(igrp, instr);

   case PCO_OP_COPYSIGN:
      return pco_copysign_map_igrp(igrp, instr);

   case PCO_OP_IBFE:
      return pco_ibfe_map_igrp(igrp, instr);

   case PCO_OP_UBFE:
      return pco_ubfe_map_igrp(igrp, instr);

   case PCO_OP_BFI:
      return pco_bfi_map_igrp(igrp, instr);

   case PCO_OP_WOP:
      return pco_wop_map_igrp(igrp, instr);

   case PCO_OP_WDF:
      return pco_wdf_map_igrp(igrp, instr);

   case PCO_OP_NOP:
      return pco_nop_map_igrp(igrp, instr);

   case PCO_OP_DITR:
      return pco_ditr_map_igrp(igrp, instr);

   case PCO_OP_DITRP:
      return pco_ditrp_map_igrp(igrp, instr);

   case PCO_OP_CNDST:
      return pco_cndst_map_igrp(igrp, instr);

   case PCO_OP_CNDEF:
      return pco_cndef_map_igrp(igrp, instr);

   case PCO_OP_CNDSM:
      return pco_cndsm_map_igrp(igrp, instr);

   case PCO_OP_CNDLT:
      return pco_cndlt_map_igrp(igrp, instr);

   case PCO_OP_CNDEND:
      return pco_cndend_map_igrp(igrp, instr);

   case PCO_OP_BR:
      return pco_br_map_igrp(igrp, instr);

   case PCO_OP_BR_NEXT:
      return pco_br_next_map_igrp(igrp, instr);

   case PCO_OP_BR_SKIP_NEXT:
      return pco_br_skip_next_map_igrp(igrp, instr);

   case PCO_OP_MUTEX:
      return pco_mutex_map_igrp(igrp, instr);

   case PCO_OP_SAVL:
      return pco_savl_map_igrp(igrp, instr);

   default:
      break;
   }

   const struct pco_op_info *info = &pco_op_info[instr->op];
   printf("Instruction group mapping not defined for %s op '%s'.\n",
          info->type == PCO_OP_TYPE_PSEUDO ? "pseudo" : "hardware",
          info->str);

   UNREACHABLE("");
}

static inline unsigned pco_igrp_hdr_map_encode(uint8_t *bin, pco_igrp *igrp)
{
   switch (igrp->variant.hdr) {
   case PCO_IGRP_HDR_MAIN_BRIEF:
      return pco_igrp_hdr_main_brief_encode(bin,
                                            .da = igrp->hdr.da,
                                            .length = igrp->hdr.length,
                                            .oporg = igrp->hdr.oporg,
                                            .olchk = igrp->hdr.olchk,
                                            .w1p = igrp->hdr.w1p,
                                            .w0p = igrp->hdr.w0p,
                                            .cc = igrp->hdr.cc);

   case PCO_IGRP_HDR_MAIN:
      return pco_igrp_hdr_main_encode(bin,
                                      .da = igrp->hdr.da,
                                      .length = igrp->hdr.length,
                                      .oporg = igrp->hdr.oporg,
                                      .olchk = igrp->hdr.olchk,
                                      .w1p = igrp->hdr.w1p,
                                      .w0p = igrp->hdr.w0p,
                                      .cc = igrp->hdr.cc,
                                      .end = igrp->hdr.end,
                                      .atom = igrp->hdr.atom,
                                      .rpt = igrp->hdr.rpt);

   case PCO_IGRP_HDR_BITWISE:
      return pco_igrp_hdr_bitwise_encode(bin,
                                         .da = igrp->hdr.da,
                                         .length = igrp->hdr.length,
                                         .opcnt = igrp->hdr.opcnt,
                                         .olchk = igrp->hdr.olchk,
                                         .w1p = igrp->hdr.w1p,
                                         .w0p = igrp->hdr.w0p,
                                         .cc = igrp->hdr.cc,
                                         .end = igrp->hdr.end,
                                         .atom = igrp->hdr.atom,
                                         .rpt = igrp->hdr.rpt);

   case PCO_IGRP_HDR_CONTROL:
      return pco_igrp_hdr_control_encode(bin,
                                         .da = igrp->hdr.da,
                                         .length = igrp->hdr.length,
                                         .olchk = igrp->hdr.olchk,
                                         .w1p = igrp->hdr.w1p,
                                         .w0p = igrp->hdr.w0p,
                                         .cc = igrp->hdr.cc,
                                         .miscctl = igrp->hdr.miscctl,
                                         .ctrlop = igrp->hdr.ctrlop);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fadd_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_fadd_encode(bin, .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s1abs = instr->src[1].abs, .s0flr = instr->src[0].flr);
}

static inline
unsigned pco_fmul_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_fmul_encode(bin, .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s1abs = instr->src[1].abs, .s0flr = instr->src[0].flr);
}

static inline
unsigned pco_fmad_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_FMAD_EXT:
      return pco_main_fmad_ext_encode(bin, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s2neg = instr->src[2].neg, .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT), .lp = pco_instr_get_mod(instr, PCO_OP_MOD_LP), .s1abs = instr->src[1].abs, .s1neg = instr->src[1].neg, .s2flr = instr->src[2].flr, .s2abs = instr->src[2].abs);

   case PCO_MAIN_FMAD:
      return pco_main_fmad_encode(bin, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s2neg = instr->src[2].neg, .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT));

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_frcp_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_RCP, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_RCP);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_frsq_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_RSQ, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_RSQ);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_flog_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_LOG, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_LOG);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_flogcn_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_LOGCN, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_LOGCN);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fexp_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_EXP, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_EXP);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fred_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_fred_encode(bin, .red_part = pco_map_fred_part_to_red_part(pco_instr_get_mod(instr, PCO_OP_MOD_FRED_PART)), .iter = pco_ref_get_imm(instr->src[2]), .red_type = pco_map_fred_type_to_red_type(pco_instr_get_mod(instr, PCO_OP_MOD_FRED_TYPE)), .pwen = !pco_ref_is_null(instr->dest[2]), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);
}

static inline
unsigned pco_fsinc_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_SINC);
}

static inline
unsigned pco_mbyp_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_BYP, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_BYP);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fdsx_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_DSX, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_DSX);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fdsxf_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_DSXF, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_DSX);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fdsy_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_DSY, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_DSY);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_fdsyf_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_SNGL_EXT:
      return pco_main_sngl_ext_encode(bin, .sngl_op = PCO_SNGL_OP_DSYF, .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs);

   case PCO_MAIN_SNGL:
      return pco_main_sngl_encode(bin, .sngl_op = PCO_SNGL_OP_DSY);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_pck_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_pck_encode(bin, .prog = false, .rtz = pco_instr_get_mod(instr, PCO_OP_MOD_ROUNDZERO), .scale = pco_instr_get_mod(instr, PCO_OP_MOD_SCALE), .pck_format = pco_map_pck_fmt_to_pck_format(pco_instr_get_mod(instr, PCO_OP_MOD_PCK_FMT)));
}

static inline
unsigned pco_pck_prog_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_pck_encode(bin, .prog = true, .rtz = pco_instr_get_mod(instr, PCO_OP_MOD_ROUNDZERO), .scale = pco_instr_get_mod(instr, PCO_OP_MOD_SCALE), .pck_format = 0);
}

static inline
unsigned pco_unpck_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_upck_encode(bin, .elem = pco_map_elem_to_upck_elem(instr->src[0].elem), .scale_rtz = pco_instr_get_mod(instr, PCO_OP_MOD_SCALE) || pco_instr_get_mod(instr, PCO_OP_MOD_ROUNDZERO), .pck_format = pco_map_pck_fmt_to_pck_format(pco_instr_get_mod(instr, PCO_OP_MOD_PCK_FMT)));
}

static inline
unsigned pco_tst_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_TST_EXT:
      return pco_main_tst_ext_encode(bin, .tst_op = pco_map_tst_op_main_to_tst_op(pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN)), .pwen = !pco_ref_is_null(instr->dest[1]), .type = pco_map_tst_type_main_to_tst_type(pco_instr_get_mod(instr, PCO_OP_MOD_TST_TYPE_MAIN)), .p2end = pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END), .elem = instr->src[0].elem);

   case PCO_MAIN_TST:
      return pco_main_tst_encode(bin, .tst_op = pco_map_tst_op_main_to_tst_op(pco_instr_get_mod(instr, PCO_OP_MOD_TST_OP_MAIN)), .pwen = !pco_ref_is_null(instr->dest[1]));

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_movc_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_MOVC_EXT:
      return pco_main_movc_ext_encode(bin, .movw0 = pco_ref_get_movw01(instr->src[1]), .movw1 = pco_ref_get_movw01(instr->src[3]), .maskw0 = pco_map_elem_to_maskw0(instr->dest[0].elem), .aw = false, .p2end = pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END));

   case PCO_MAIN_MOVC:
      return pco_main_movc_encode(bin, .movw0 = pco_ref_get_movw01(instr->src[1]), .movw1 = pco_ref_get_movw01(instr->src[3]));

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_movwm_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_main_movc_ext_encode(bin, .movw0 = pco_ref_get_movw01(instr->src[0]), .movw1 = 0, .maskw0 = pco_map_elem_to_maskw0(instr->dest[0].elem), .aw = true, .p2end = pco_instr_get_mod(instr, PCO_OP_MOD_PHASE2END));
}

static inline
unsigned pco_add64_32_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_INT32_64_EXT:
      return pco_main_int32_64_ext_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_ADD6432, .cin = !pco_ref_is_null(instr->src[3]), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s1neg = false, .s1abs = false, .s2neg = instr->src[2].neg, .s2abs = instr->src[2].abs);

   case PCO_MAIN_INT32_64:
      return pco_main_int32_64_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_ADD6432, .s2neg = instr->src[2].neg);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_imadd64_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_INT32_64_EXT:
      return pco_main_int32_64_ext_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_MADD64, .cin = !pco_ref_is_null(instr->src[4]), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s1neg = instr->src[1].neg, .s1abs = instr->src[1].abs, .s2neg = instr->src[2].neg, .s2abs = instr->src[2].abs);

   case PCO_MAIN_INT32_64:
      return pco_main_int32_64_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_MADD64, .s2neg = instr->src[2].neg);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_imadd32_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_MAIN_INT32_64_EXT:
      return pco_main_int32_64_ext_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_MADD32, .cin = !pco_ref_is_null(instr->src[3]), .s0neg = instr->src[0].neg, .s0abs = instr->src[0].abs, .s1neg = instr->src[1].neg, .s1abs = instr->src[1].abs, .s2neg = instr->src[2].neg, .s2abs = instr->src[2].abs);

   case PCO_MAIN_INT32_64:
      return pco_main_int32_64_encode(bin, .s = pco_instr_get_mod(instr, PCO_OP_MOD_S), .int32_64_op = PCO_INT32_64_OP_MADD32, .s2neg = instr->src[2].neg);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_uvsw_write_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_uvsw_write_imm_encode(bin, .dsel = PCO_DSEL_W0, .imm_addr = pco_ref_get_imm(instr->src[1]));
}

static inline
unsigned pco_uvsw_emit_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_uvsw_emit_encode(bin);
}

static inline
unsigned pco_uvsw_endtask_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_uvsw_endtask_encode(bin);
}

static inline
unsigned pco_uvsw_emit_endtask_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_uvsw_emit_endtask_encode(bin);
}

static inline
unsigned pco_uvsw_write_emit_endtask_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_uvsw_write_emit_endtask_imm_encode(bin, .dsel = PCO_DSEL_W0, .imm_addr = pco_ref_get_imm(instr->src[1]));
}

static inline
unsigned pco_fitr_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_fitr_encode(bin, .p = false, .drc = pco_ref_get_drc(instr->src[0]), .iter_mode = pco_map_itr_mode_to_iter_mode(pco_instr_get_mod(instr, PCO_OP_MOD_ITR_MODE)), .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT), .count = pco_ref_get_imm(instr->src[2]));
}

static inline
unsigned pco_fitrp_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_fitr_encode(bin, .p = true, .drc = pco_ref_get_drc(instr->src[0]), .iter_mode = pco_map_itr_mode_to_iter_mode(pco_instr_get_mod(instr, PCO_OP_MOD_ITR_MODE)), .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT), .count = pco_ref_get_imm(instr->src[3]));
}

static inline
unsigned pco_ld_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_ld_immbl_encode(bin, .drc = pco_ref_get_drc(instr->src[0]), .burstlen = pco_ref_get_imm(instr->src[1]), .srcseladd = pco_ref_srcsel(instr->src[2]), .cachemode_ld = pco_map_mcu_cache_mode_ld_to_cachemode_ld(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD)));
}

static inline
unsigned pco_ld_regbl_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_ld_regbl_encode(bin, .drc = pco_ref_get_drc(instr->src[0]), .srcseladd = pco_ref_srcsel(instr->src[2]), .srcselbl = pco_ref_srcsel(instr->src[1]), .cachemode_ld = pco_map_mcu_cache_mode_ld_to_cachemode_ld(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD)));
}

static inline
unsigned pco_st_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_st_immbl_encode(bin, .drc = pco_ref_get_drc(instr->src[2]), .srcseladd = pco_ref_srcsel(instr->src[4]), .burstlen = pco_ref_get_imm(instr->src[3]), .cachemode_st = pco_map_mcu_cache_mode_st_to_cachemode_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST)), .srcseldata = pco_ref_srcsel(instr->src[0]), .dsize = pco_ref_get_imm(instr->src[1]));
}

static inline
unsigned pco_st_regbl_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_st_regbl_encode(bin, .drc = pco_ref_get_drc(instr->src[2]), .srcseladd = pco_ref_srcsel(instr->src[4]), .srcselbl = pco_ref_srcsel(instr->src[3]), .cachemode_st = pco_map_mcu_cache_mode_st_to_cachemode_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST)), .srcseldata = pco_ref_srcsel(instr->src[0]), .dsize = pco_ref_get_imm(instr->src[1]));
}

static inline
unsigned pco_st_tiled_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_st_immbl_tiled_encode(bin, .drc = pco_ref_get_drc(instr->src[2]), .srcseladd = pco_ref_srcsel(instr->src[4]), .burstlen = pco_ref_get_imm(instr->src[3]), .cachemode_st = pco_map_mcu_cache_mode_st_to_cachemode_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST)), .srcseldata = pco_ref_srcsel(instr->src[0]), .dsize = pco_ref_get_imm(instr->src[1]), .srcmask = pco_ref_srcsel(instr->src[5]));
}

static inline
unsigned pco_idf_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_idf_encode(bin, .drc = pco_ref_get_drc(instr->src[0]), .srcseladd = pco_ref_srcsel(instr->src[1]));
}

static inline
unsigned pco_atomic_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_atomic_encode(bin, .drc = pco_ref_get_drc(instr->src[0]), .atomic_op = pco_map_atom_op_to_atomic_op(pco_instr_get_mod(instr, PCO_OP_MOD_ATOM_OP)), .srcsel = pco_ref_srcsel(instr->src[1]), .dstsel = pco_ref_srcsel(instr->dest[0]));
}

static inline
unsigned pco_smp_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_BACKEND_SMP_EXTABC:
      return pco_backend_smp_extabc_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)), .pplod = pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD), .proj = pco_instr_get_mod(instr, PCO_OP_MOD_PROJ), .sbmode = pco_map_sb_mode_to_sbmode(pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE)), .nncoords = pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS), .sno = pco_instr_get_mod(instr, PCO_OP_MOD_SNO), .soo = pco_instr_get_mod(instr, PCO_OP_MOD_SOO), .tao = pco_instr_get_mod(instr, PCO_OP_MOD_TAO), .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .swap = pco_map_schedswap_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP)), .cachemode_ld_st = pco_map_mcu_cache_mode_ld_to_cachemode_ld_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD)), .w = false, .integer = pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER), .array = pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY));

   case PCO_BACKEND_SMP_BRIEF:
      return pco_backend_smp_brief_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)));

   case PCO_BACKEND_SMP_EXTA:
      return pco_backend_smp_exta_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)), .pplod = pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD), .proj = pco_instr_get_mod(instr, PCO_OP_MOD_PROJ), .sbmode = pco_map_sb_mode_to_sbmode(pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE)), .nncoords = pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS), .sno = pco_instr_get_mod(instr, PCO_OP_MOD_SNO), .soo = pco_instr_get_mod(instr, PCO_OP_MOD_SOO), .tao = pco_instr_get_mod(instr, PCO_OP_MOD_TAO));

   case PCO_BACKEND_SMP_EXTAB:
      return pco_backend_smp_extab_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)), .pplod = pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD), .proj = pco_instr_get_mod(instr, PCO_OP_MOD_PROJ), .sbmode = pco_map_sb_mode_to_sbmode(pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE)), .nncoords = pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS), .sno = pco_instr_get_mod(instr, PCO_OP_MOD_SNO), .soo = pco_instr_get_mod(instr, PCO_OP_MOD_SOO), .tao = pco_instr_get_mod(instr, PCO_OP_MOD_TAO), .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .swap = pco_map_schedswap_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP)), .cachemode_ld_st = pco_map_mcu_cache_mode_ld_to_cachemode_ld_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_LD)), .w = false);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_smp_wrt_map_encode(uint8_t *bin, pco_instr *instr, unsigned variant)
{
   switch (variant) {
   case PCO_BACKEND_SMP_EXTABC:
      return pco_backend_smp_extabc_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)), .pplod = pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD), .proj = pco_instr_get_mod(instr, PCO_OP_MOD_PROJ), .sbmode = pco_map_sb_mode_to_sbmode(pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE)), .nncoords = pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS), .sno = pco_instr_get_mod(instr, PCO_OP_MOD_SNO), .soo = pco_instr_get_mod(instr, PCO_OP_MOD_SOO), .tao = pco_instr_get_mod(instr, PCO_OP_MOD_TAO), .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .swap = pco_map_schedswap_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP)), .cachemode_ld_st = pco_map_mcu_cache_mode_st_to_cachemode_ld_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST)), .w = true, .integer = pco_instr_get_mod(instr, PCO_OP_MOD_INTEGER), .array = pco_instr_get_mod(instr, PCO_OP_MOD_ARRAY));

   case PCO_BACKEND_SMP_EXTAB:
      return pco_backend_smp_extab_encode(bin, .fcnorm = pco_instr_get_mod(instr, PCO_OP_MOD_FCNORM), .drc = pco_ref_get_drc(instr->src[0]), .dmn = pco_map_dim_to_dmn(pco_instr_get_mod(instr, PCO_OP_MOD_DIM)), .chan = pco_ref_get_imm(instr->src[5]), .lodm = pco_map_lod_mode_to_lodm(pco_instr_get_mod(instr, PCO_OP_MOD_LOD_MODE)), .pplod = pco_instr_get_mod(instr, PCO_OP_MOD_PPLOD), .proj = pco_instr_get_mod(instr, PCO_OP_MOD_PROJ), .sbmode = pco_map_sb_mode_to_sbmode(pco_instr_get_mod(instr, PCO_OP_MOD_SB_MODE)), .nncoords = pco_instr_get_mod(instr, PCO_OP_MOD_NNCOORDS), .sno = pco_instr_get_mod(instr, PCO_OP_MOD_SNO), .soo = pco_instr_get_mod(instr, PCO_OP_MOD_SOO), .tao = pco_instr_get_mod(instr, PCO_OP_MOD_TAO), .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .swap = pco_map_schedswap_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHEDSWAP)), .cachemode_ld_st = pco_map_mcu_cache_mode_st_to_cachemode_ld_st(pco_instr_get_mod(instr, PCO_OP_MOD_MCU_CACHE_MODE_ST)), .w = true);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_alphatst_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_vistest_atst_encode(bin, .pwen = !pco_ref_is_null(instr->dest[0]), .ifb = true);
}

static inline
unsigned pco_alphaf_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_vistest_atst_encode(bin, .pwen = !pco_ref_is_null(instr->dest[0]), .ifb = false);
}

static inline
unsigned pco_depthf_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_vistest_depthf_encode(bin);
}

static inline
unsigned pco_savmsk_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_savmsk_encode(bin, .msk_mode = pco_map_savmsk_mode_to_msk_mode(pco_instr_get_mod(instr, PCO_OP_MOD_SAVMSK_MODE)));
}

static inline
unsigned pco_emitpix_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_emitpix_encode(bin, .freep = pco_instr_get_mod(instr, PCO_OP_MOD_FREEP));
}

static inline
unsigned pco_setl_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_backend_setl_encode(bin, .ressel = PCO_DSEL_W0);
}

static inline
unsigned pco_bbyp0bm_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_S2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_BYP);
}

static inline
unsigned pco_bbyp0bm_imm32_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_imm32_encode(bin, .count_src = PCO_COUNT_SRC_S2, .count_op = PCO_COUNT_OP_BYP, .shift1_op = PCO_SHIFT1_OP_BYP, .imm32 = pco_ref_get_imm(instr->src[1]));
}

static inline
unsigned pco_bbyp0s1_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_S2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_BYP);
}

static inline
unsigned pco_msk_bbyp0s1_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_S2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_MSK, .shift1_op = PCO_SHIFT1_OP_BYP);
}

static inline
unsigned pco_msk_lsl_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_S2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_MSK, .shift1_op = PCO_SHIFT1_OP_LSL);
}

static inline
unsigned pco_cbs_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = pco_ref_count_src(instr->src[0]), .count_op = PCO_COUNT_OP_CBS, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_BYP);
}

static inline
unsigned pco_ftb_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = pco_ref_count_src(instr->src[0]), .count_op = PCO_COUNT_OP_FTB, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_BYP);
}

static inline
unsigned pco_rev_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_FT2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_REV);
}

static inline
unsigned pco_shuffle_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase0_src_encode(bin, .count_src = PCO_COUNT_SRC_FT2, .count_op = PCO_COUNT_OP_BYP, .bitmask_src_op = PCO_BITMASK_SRC_OP_BYP, .shift1_op = PCO_SHIFT1_OP_SHFL);
}

static inline
unsigned pco_logical_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase1_encode(bin, .mskb = !pco_ref_is_null(instr->src[2]), .mska = !pco_ref_is_null(instr->src[0]), .logical_op = pco_map_logiop_to_logical_op(pco_instr_get_mod(instr, PCO_OP_MOD_LOGIOP)));
}

static inline
unsigned pco_shift_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_bitwise_phase2_encode(bin, .pwen = false, .tst_src = 0, .bw_tst_op = 0, .shift2_op = pco_map_shiftop_to_shift2_op(pco_instr_get_mod(instr, PCO_OP_MOD_SHIFTOP)));
}

static inline
unsigned pco_wop_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_wop_encode(bin);
}

static inline
unsigned pco_wdf_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_wdf_encode(bin);
}

static inline
unsigned pco_nop_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_nop_encode(bin);
}

static inline
unsigned pco_ditr_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_ditr_encode(bin, .dest = pco_ref_get_temp(instr->dest[0]), .coff = pco_ref_get_coeff(instr->src[1]), .p = PCO_PERSP_CTL_NONE, .woff = 0, .mode = pco_map_itr_mode_to_iter_mode(pco_instr_get_mod(instr, PCO_OP_MOD_ITR_MODE)), .count = pco_ref_get_imm(instr->src[2]), .coff_idx_ctrl = pco_ref_get_reg_idx_ctrl(instr->src[1]), .woff_idx_ctrl = PCO_IDX_CTRL_NONE, .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .sched_ctrl = pco_map_sched_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHED)), .drc = pco_ref_get_drc(instr->src[0]), .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT));
}

static inline
unsigned pco_ditrp_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_ditr_encode(bin, .dest = pco_ref_get_temp(instr->dest[0]), .coff = pco_ref_get_coeff(instr->src[1]), .p = PCO_PERSP_CTL_ITER_MUL, .woff = pco_ref_get_coeff(instr->src[2]), .mode = pco_map_itr_mode_to_iter_mode(pco_instr_get_mod(instr, PCO_OP_MOD_ITR_MODE)), .count = pco_ref_get_imm(instr->src[3]), .coff_idx_ctrl = pco_ref_get_reg_idx_ctrl(instr->src[1]), .woff_idx_ctrl = pco_ref_get_reg_idx_ctrl(instr->src[2]), .f16 = pco_instr_get_mod(instr, PCO_OP_MOD_F16), .sched_ctrl = pco_map_sched_to_sched_ctrl(pco_instr_get_mod(instr, PCO_OP_MOD_SCHED)), .drc = pco_ref_get_drc(instr->src[0]), .sat = pco_instr_get_mod(instr, PCO_OP_MOD_SAT));
}

static inline
unsigned pco_cndst_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_cnd_encode(bin, .adjust = pco_ref_get_imm(instr->src[1]), .pcnd = pco_map_cnd_to_pcnd(pco_instr_get_mod(instr, PCO_OP_MOD_CND)), .cndinst = PCO_CNDINST_ST);
}

static inline
unsigned pco_cndef_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_cnd_encode(bin, .adjust = pco_ref_get_imm(instr->src[1]), .pcnd = pco_map_cnd_to_pcnd(pco_instr_get_mod(instr, PCO_OP_MOD_CND)), .cndinst = PCO_CNDINST_EF);
}

static inline
unsigned pco_cndsm_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_cnd_encode(bin, .adjust = 0, .pcnd = pco_map_cnd_to_pcnd(pco_instr_get_mod(instr, PCO_OP_MOD_CND)), .cndinst = PCO_CNDINST_SM);
}

static inline
unsigned pco_cndlt_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_cnd_encode(bin, .adjust = pco_ref_get_imm(instr->src[1]), .pcnd = pco_map_cnd_to_pcnd(pco_instr_get_mod(instr, PCO_OP_MOD_CND)), .cndinst = PCO_CNDINST_LT);
}

static inline
unsigned pco_cndend_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_cnd_encode(bin, .adjust = pco_ref_get_imm(instr->src[1]), .pcnd = 0, .cndinst = PCO_CNDINST_END);
}

static inline
unsigned pco_br_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_branch_encode(bin, .link = pco_instr_get_mod(instr, PCO_OP_MOD_LINK), .bpred = pco_map_branch_cnd_to_bpred(pco_instr_get_mod(instr, PCO_OP_MOD_BRANCH_CND)), .abs = false, .offset = pco_branch_rel_offset(instr->parent_igrp, instr->target_cf_node));
}

static inline
unsigned pco_br_next_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_branch_encode(bin, .link = false, .bpred = PCO_BPRED_CC, .abs = false, .offset = pco_branch_rel_offset_next_igrp(instr->parent_igrp, false));
}

static inline
unsigned pco_br_skip_next_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_branch_encode(bin, .link = false, .bpred = pco_map_branch_cnd_to_bpred(pco_instr_get_mod(instr, PCO_OP_MOD_BRANCH_CND)), .abs = false, .offset = pco_branch_rel_offset_next_igrp(instr->parent_igrp, true));
}

static inline
unsigned pco_mutex_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_mutex_encode(bin, .lr = pco_map_mutex_op_to_lr(pco_instr_get_mod(instr, PCO_OP_MOD_MUTEX_OP)), .id = pco_ref_get_imm(instr->src[0]));
}

static inline
unsigned pco_savl_map_encode(uint8_t *bin, pco_instr *instr)
{
   return pco_ctrl_savl_encode(bin);
}

static inline
unsigned pco_instr_map_encode(uint8_t *bin, pco_igrp *igrp, enum pco_op_phase phase)
{
   pco_instr *instr = igrp->instrs[phase];
   switch (instr->op) {
   case PCO_OP_FADD:
      return pco_fadd_map_encode(bin, instr);

   case PCO_OP_FMUL:
      return pco_fmul_map_encode(bin, instr);

   case PCO_OP_FMAD:
      return pco_fmad_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FRCP:
      return pco_frcp_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FRSQ:
      return pco_frsq_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FLOG:
      return pco_flog_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FLOGCN:
      return pco_flogcn_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FEXP:
      return pco_fexp_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FRED:
      return pco_fred_map_encode(bin, instr);

   case PCO_OP_FSINC:
      return pco_fsinc_map_encode(bin, instr);

   case PCO_OP_MBYP:
      return pco_mbyp_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FDSX:
      return pco_fdsx_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FDSXF:
      return pco_fdsxf_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FDSY:
      return pco_fdsy_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_FDSYF:
      return pco_fdsyf_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_PCK:
      return pco_pck_map_encode(bin, instr);

   case PCO_OP_PCK_PROG:
      return pco_pck_prog_map_encode(bin, instr);

   case PCO_OP_UNPCK:
      return pco_unpck_map_encode(bin, instr);

   case PCO_OP_TST:
      return pco_tst_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_MOVC:
      return pco_movc_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_MOVWM:
      return pco_movwm_map_encode(bin, instr);

   case PCO_OP_ADD64_32:
      return pco_add64_32_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_IMADD64:
      return pco_imadd64_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_IMADD32:
      return pco_imadd32_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_UVSW_WRITE:
      return pco_uvsw_write_map_encode(bin, instr);

   case PCO_OP_UVSW_EMIT:
      return pco_uvsw_emit_map_encode(bin, instr);

   case PCO_OP_UVSW_ENDTASK:
      return pco_uvsw_endtask_map_encode(bin, instr);

   case PCO_OP_UVSW_EMIT_ENDTASK:
      return pco_uvsw_emit_endtask_map_encode(bin, instr);

   case PCO_OP_UVSW_WRITE_EMIT_ENDTASK:
      return pco_uvsw_write_emit_endtask_map_encode(bin, instr);

   case PCO_OP_FITR:
      return pco_fitr_map_encode(bin, instr);

   case PCO_OP_FITRP:
      return pco_fitrp_map_encode(bin, instr);

   case PCO_OP_LD:
      return pco_ld_map_encode(bin, instr);

   case PCO_OP_LD_REGBL:
      return pco_ld_regbl_map_encode(bin, instr);

   case PCO_OP_ST:
      return pco_st_map_encode(bin, instr);

   case PCO_OP_ST_REGBL:
      return pco_st_regbl_map_encode(bin, instr);

   case PCO_OP_ST_TILED:
      return pco_st_tiled_map_encode(bin, instr);

   case PCO_OP_IDF:
      return pco_idf_map_encode(bin, instr);

   case PCO_OP_ATOMIC:
      return pco_atomic_map_encode(bin, instr);

   case PCO_OP_SMP:
      return pco_smp_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_SMP_WRT:
      return pco_smp_wrt_map_encode(bin, instr, pco_igrp_variant(igrp, phase));

   case PCO_OP_ALPHATST:
      return pco_alphatst_map_encode(bin, instr);

   case PCO_OP_ALPHAF:
      return pco_alphaf_map_encode(bin, instr);

   case PCO_OP_DEPTHF:
      return pco_depthf_map_encode(bin, instr);

   case PCO_OP_SAVMSK:
      return pco_savmsk_map_encode(bin, instr);

   case PCO_OP_EMITPIX:
      return pco_emitpix_map_encode(bin, instr);

   case PCO_OP_SETL:
      return pco_setl_map_encode(bin, instr);

   case PCO_OP_BBYP0BM:
      return pco_bbyp0bm_map_encode(bin, instr);

   case PCO_OP_BBYP0BM_IMM32:
      return pco_bbyp0bm_imm32_map_encode(bin, instr);

   case PCO_OP_BBYP0S1:
      return pco_bbyp0s1_map_encode(bin, instr);

   case PCO_OP_MSK_BBYP0S1:
      return pco_msk_bbyp0s1_map_encode(bin, instr);

   case PCO_OP_MSK_LSL:
      return pco_msk_lsl_map_encode(bin, instr);

   case PCO_OP_CBS:
      return pco_cbs_map_encode(bin, instr);

   case PCO_OP_FTB:
      return pco_ftb_map_encode(bin, instr);

   case PCO_OP_REV:
      return pco_rev_map_encode(bin, instr);

   case PCO_OP_SHUFFLE:
      return pco_shuffle_map_encode(bin, instr);

   case PCO_OP_LOGICAL:
      return pco_logical_map_encode(bin, instr);

   case PCO_OP_SHIFT:
      return pco_shift_map_encode(bin, instr);

   case PCO_OP_WOP:
      return pco_wop_map_encode(bin, instr);

   case PCO_OP_WDF:
      return pco_wdf_map_encode(bin, instr);

   case PCO_OP_NOP:
      return pco_nop_map_encode(bin, instr);

   case PCO_OP_DITR:
      return pco_ditr_map_encode(bin, instr);

   case PCO_OP_DITRP:
      return pco_ditrp_map_encode(bin, instr);

   case PCO_OP_CNDST:
      return pco_cndst_map_encode(bin, instr);

   case PCO_OP_CNDEF:
      return pco_cndef_map_encode(bin, instr);

   case PCO_OP_CNDSM:
      return pco_cndsm_map_encode(bin, instr);

   case PCO_OP_CNDLT:
      return pco_cndlt_map_encode(bin, instr);

   case PCO_OP_CNDEND:
      return pco_cndend_map_encode(bin, instr);

   case PCO_OP_BR:
      return pco_br_map_encode(bin, instr);

   case PCO_OP_BR_NEXT:
      return pco_br_next_map_encode(bin, instr);

   case PCO_OP_BR_SKIP_NEXT:
      return pco_br_skip_next_map_encode(bin, instr);

   case PCO_OP_MUTEX:
      return pco_mutex_map_encode(bin, instr);

   case PCO_OP_SAVL:
      return pco_savl_map_encode(bin, instr);

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_srcs_map_encode(uint8_t *bin, pco_igrp *igrp, bool is_upper)
{
   unsigned offset = is_upper ? ROGUE_ALU_INPUT_GROUP_SIZE : 0;

   pco_ref _sA = igrp->srcs.s[0 + offset];
   pco_ref _sB = igrp->srcs.s[1 + offset];
   pco_ref _sC = igrp->srcs.s[2 + offset];
   pco_ref _mux = is_upper ? pco_ref_null() : igrp->iss.is[0];

   bool sA_set = !pco_ref_is_null(_sA);
   bool sB_set = !pco_ref_is_null(_sB);
   bool sC_set = !pco_ref_is_null(_sC);
   bool mux_set = !pco_ref_is_null(_mux);

   unsigned sbA = sA_set ? pco_map_reg_bank(_sA) : 0;
   unsigned sA = sA_set ? pco_map_reg_index(_sA) : 0;

   unsigned sbB = sB_set ? pco_map_reg_bank(_sB) : 0;
   unsigned sB = sB_set ? pco_map_reg_index(_sB) : 0;

   unsigned sbC = sC_set ? pco_map_reg_bank(_sC) : 0;
   unsigned sC = sC_set ? pco_map_reg_index(_sC) : 0;

   unsigned mux = mux_set ? pco_map_io_to_is0_sel(pco_ref_get_io(_mux)) : 0;

   enum pco_src_variant variant = is_upper ? igrp->variant.upper_src : igrp->variant.lower_src;
   switch (variant) {
   case PCO_SRC_1LO_1B6I:
      return pco_src_1lo_1b6i_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
      );

   case PCO_SRC_1LO_3B11I_2M:
      return pco_src_1lo_3b11i_2m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .is0 = mux,
      );

   case PCO_SRC_2LO_1B6I_1B5I:
      return pco_src_2lo_1b6i_1b5i_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
      );

   case PCO_SRC_2LO_2B7I_2B7I_2M:
      return pco_src_2lo_2b7i_2b7i_2m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
         .is0 = mux,
      );

   case PCO_SRC_2LO_3B11I_2B8I_3M:
      return pco_src_2lo_3b11i_2b8i_3m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
         .is0 = mux,
      );

   case PCO_SRC_3LO_2B7I_2B7I_2B6I_2M:
      return pco_src_3lo_2b7i_2b7i_2b6i_2m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
         .s2 = sC,
         .sb2 = sbC,
         .is0 = mux,
      );

   case PCO_SRC_3LO_3B8I_2B8I_3B8I_3M:
      return pco_src_3lo_3b8i_2b8i_3b8i_3m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
         .s2 = sC,
         .sb2 = sbC,
         .is0 = mux,
      );

   case PCO_SRC_3LO_3B11I_2B8I_3B11I_3M:
      return pco_src_3lo_3b11i_2b8i_3b11i_3m_encode(bin,
         .s0 = sA,
         .sb0 = sbA,
         .s1 = sB,
         .sb1 = sbB,
         .s2 = sC,
         .sb2 = sbC,
         .is0 = mux,
      );

   case PCO_SRC_1UP_1B6I:
      return pco_src_1up_1b6i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
      );

   case PCO_SRC_1UP_3B11I:
      return pco_src_1up_3b11i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
      );

   case PCO_SRC_2UP_1B6I_1B5I:
      return pco_src_2up_1b6i_1b5i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
      );

   case PCO_SRC_2UP_2B7I_2B7I:
      return pco_src_2up_2b7i_2b7i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
      );

   case PCO_SRC_2UP_3B11I_2B8I:
      return pco_src_2up_3b11i_2b8i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
      );

   case PCO_SRC_3UP_2B7I_2B7I_2B6I:
      return pco_src_3up_2b7i_2b7i_2b6i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
         .s5 = sC,
         .sb5 = sbC,
      );

   case PCO_SRC_3UP_3B8I_2B8I_2B8I:
      return pco_src_3up_3b8i_2b8i_2b8i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
         .s5 = sC,
         .sb5 = sbC,
      );

   case PCO_SRC_3UP_3B11I_2B8I_2B8I:
      return pco_src_3up_3b11i_2b8i_2b8i_encode(bin,
         .s3 = sA,
         .sb3 = sbA,
         .s4 = sB,
         .sb4 = sbB,
         .s5 = sC,
         .sb5 = sbC,
      );

   default:
      break;
   }

   UNREACHABLE("");
}

static inline
unsigned pco_iss_map_encode(uint8_t *bin, pco_igrp *igrp)
{
   bool is5_set = !pco_ref_is_null(igrp->iss.is[5]);
   bool is4_set = !pco_ref_is_null(igrp->iss.is[4]);
   bool is3_set = !pco_ref_is_null(igrp->iss.is[3]);
   bool is2_set = !pco_ref_is_null(igrp->iss.is[2]);
   bool is1_set = !pco_ref_is_null(igrp->iss.is[1]);

   unsigned is5 = is5_set ? pco_map_io_to_is5_sel(pco_ref_get_io(igrp->iss.is[5])) : 0;
   unsigned is4 = is4_set ? pco_map_io_to_is4_sel(pco_ref_get_io(igrp->iss.is[4])) : 0;
   unsigned is3 = is3_set ? pco_map_io_to_is3_sel(pco_ref_get_io(igrp->iss.is[3])) : 0;
   unsigned is2 = is2_set ? pco_map_io_to_is2_sel(pco_ref_get_io(igrp->iss.is[2])) : 0;
   unsigned is1 = is1_set ? pco_map_io_to_is1_sel(pco_ref_get_io(igrp->iss.is[1])) : 0;

   assert(igrp->variant.iss == PCO_ISS_ISS);
   return pco_iss_iss_encode(bin, .is5 = is5, .is4 = is4, .is3 = is3, .is2 = is2, .is1 = is1);
}

static inline
unsigned pco_dests_map_encode(uint8_t *bin, pco_igrp *igrp)
{
   pco_ref w0 = igrp->dests.w[0];
   pco_ref w1 = igrp->dests.w[1];

   bool w0_set = !pco_ref_is_null(w0);
   bool w1_set = !pco_ref_is_null(w1);

   bool one_dest = w0_set != w1_set;

   int db0 = w0_set ? pco_map_reg_bank(w0) : 0;
   int d0 = w0_set ? pco_map_reg_index(w0) : 0;

   int db1 = w1_set ? pco_map_reg_bank(w1) : 0;
   int d1 = w1_set ? pco_map_reg_index(w1) : 0;

   int dbN = w0_set ? db0 : db1;
   int dN = w0_set ? d0 : d1;

   if (one_dest) {
      db0 = dbN;
      d0 = dN;

      db1 = 0;
      d1 = 0;
   }

   switch (igrp->variant.dest) {
   case PCO_DST_1_1B6I:
      return pco_dst_1_1b6i_encode(bin,
         .dN = d0,
         .dbN = db0,
      );

   case PCO_DST_1_3B11I:
      return pco_dst_1_3b11i_encode(bin,
         .dN = d0,
         .dbN = db0,
      );

   case PCO_DST_2_1B7I_1B6I:
      return pco_dst_2_1b7i_1b6i_encode(bin,
         .d0 = d0,
         .db0 = db0,
         .d1 = d1,
         .db1 = db1,
      );

   case PCO_DST_2_3B8I_3B8I:
      return pco_dst_2_3b8i_3b8i_encode(bin,
         .d0 = d0,
         .db0 = db0,
         .d1 = d1,
         .db1 = db1,
      );

   case PCO_DST_2_3B11I_3B11I:
      return pco_dst_2_3b11i_3b11i_encode(bin,
         .d0 = d0,
         .db0 = db0,
         .d1 = d1,
         .db1 = db1,
      );

   default:
      break;
   }

   UNREACHABLE("");
}
#endif /* PCO_MAP_H */

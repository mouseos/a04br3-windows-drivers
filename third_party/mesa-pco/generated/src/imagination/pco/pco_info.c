/*
 * Copyright © 2024 Imagination Technologies Ltd.
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * \file pco_info.c
 *
 * \brief PCO info structures.
 */

#include "pco_common.h"
#include "pco_internal.h"
#include "pco_isa.h"
#include "pco_ops.h"

const struct pco_op_info pco_op_info[_PCO_OP_COUNT] = {
   [PCO_OP_FADD] = {
      .str = "fadd",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_SAT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_SAT] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG) | (1ULL << PCO_REF_MOD_FLR),
         [1] = (1ULL << PCO_REF_MOD_ABS),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
            [1] = (1U << PCO_REF_MAP_S4),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FMUL] = {
      .str = "fmul",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_SAT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_SAT] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG) | (1ULL << PCO_REF_MOD_FLR),
         [1] = (1ULL << PCO_REF_MOD_ABS),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
            [1] = (1U << PCO_REF_MAP_S4),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FMAD] = {
      .str = "fmad",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_SAT) | (1ULL << PCO_OP_MOD_LP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_SAT] = 6,
         [PCO_OP_MOD_LP] = 7,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG) | (1ULL << PCO_REF_MOD_FLR),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
         [2] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
            [1] = (1U << PCO_REF_MAP_S4),
            [2] = (1U << PCO_REF_MAP_S5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FRCP] = {
      .str = "frcp",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FRSQ] = {
      .str = "frsq",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FLOG] = {
      .str = "flog",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FLOGCN] = {
      .str = "flogcn",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FEXP] = {
      .str = "fexp",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FRED] = {
      .str = "fred",
      .num_dests = 3,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_FRED_TYPE) | (1ULL << PCO_OP_MOD_FRED_PART),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_FRED_TYPE] = 6,
         [PCO_OP_MOD_FRED_PART] = 7,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_W1) | (1U << PCO_REF_MAP__),
            [2] = (1U << PCO_REF_MAP_P0) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP__),
            [2] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FSINC] = {
      .str = "fsinc",
      .num_dests = 2,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_W0),
            [1] = (1U << PCO_REF_MAP_P0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MBYP] = {
      .str = "mbyp",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FDSX] = {
      .str = "fdsx",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FDSXF] = {
      .str = "fdsxf",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FDSY] = {
      .str = "fdsy",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FDSYF] = {
      .str = "fdsyf",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_PCK] = {
      .str = "pck",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_PCK_FMT) | (1ULL << PCO_OP_MOD_ROUNDZERO) | (1ULL << PCO_OP_MOD_SCALE),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_PCK_FMT] = 6,
         [PCO_OP_MOD_ROUNDZERO] = 7,
         [PCO_OP_MOD_SCALE] = 8,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2_PCK] = {
            [0] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2_PCK] = {
            [0] = (1U << PCO_REF_MAP_IS3) | (1U << PCO_REF_MAP__),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_PCK_PROG] = {
      .str = "pck.prog",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_ROUNDZERO) | (1ULL << PCO_OP_MOD_SCALE),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_ROUNDZERO] = 6,
         [PCO_OP_MOD_SCALE] = 7,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2_PCK] = {
            [0] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2_PCK] = {
            [0] = (1U << PCO_REF_MAP_IS3),
            [1] = (1U << PCO_REF_MAP_FTE),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ADD64_32] = {
      .str = "add64_32",
      .num_dests = 2,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_S] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = 0,
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
         [2] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
            [1] = (1U << PCO_REF_MAP_FTE),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
            [3] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_P0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_IMADD64] = {
      .str = "imadd64",
      .num_dests = 2,
      .num_srcs = 5,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_S] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
         [2] = 3,
         [3] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_FTE) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
            [3] = (1U << PCO_REF_MAP_IS0),
            [4] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_P0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_IMADD32] = {
      .str = "imadd32",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_S] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
         [2] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
            [3] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_P0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_UNPCK] = {
      .str = "unpck",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_PCK_FMT) | (1ULL << PCO_OP_MOD_ROUNDZERO) | (1ULL << PCO_OP_MOD_SCALE),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_PCK_FMT] = 6,
         [PCO_OP_MOD_ROUNDZERO] = 7,
         [PCO_OP_MOD_SCALE] = 8,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ELEM),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_TST] = {
      .str = "tst",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_TST_OP_MAIN) | (1ULL << PCO_OP_MOD_PHASE2END) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_TST_OP_MAIN] = 1,
         [PCO_OP_MOD_PHASE2END] = 2,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 3,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ELEM),
         [1] = (1ULL << PCO_REF_MOD_ELEM),
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2_TST] = {
            [0] = (1U << PCO_REF_MAP_FTT),
            [1] = (1U << PCO_REF_MAP_P0) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2_TST] = {
            [0] = (1U << PCO_REF_MAP_IS1) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_IS2) | (1U << PCO_REF_MAP__),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MOVC] = {
      .str = "movc",
      .num_dests = 2,
      .num_srcs = 5,
      .mods = (1ULL << PCO_OP_MOD_PHASE2END),
      .mod_map = {
         [PCO_OP_MOD_PHASE2END] = 1,
      },
      .dest_mods = {
         [0] = (1ULL << PCO_REF_MOD_ELEM),
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2_MOV] = {
            [0] = (1U << PCO_REF_MAP_W0) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_W1) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2_MOV] = {
            [0] = (1U << PCO_REF_MAP_FTT),
            [1] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_FT0) | (1U << PCO_REF_MAP_FT1) | (1U << PCO_REF_MAP_FT2) | (1U << PCO_REF_MAP_FTE),
            [2] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_IS4),
            [3] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_FT0) | (1U << PCO_REF_MAP_FT1) | (1U << PCO_REF_MAP_FT2) | (1U << PCO_REF_MAP_FTE),
            [4] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_IS5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MOVWM] = {
      .str = "movwm",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_PHASE2END),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_PHASE2END] = 6,
      },
      .dest_mods = {
         [0] = (1ULL << PCO_REF_MOD_ELEM),
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2_MOV] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2_MOV] = {
            [0] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_FT0) | (1U << PCO_REF_MAP_FT1) | (1U << PCO_REF_MAP_FT2) | (1U << PCO_REF_MAP_FTE),
            [1] = (1U << PCO_REF_MAP_IS4),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MOVS1] = {
      .str = "movs1",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 2,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_UVSW_WRITE] = {
      .str = "uvsw.write",
      .num_dests = 0,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_RPT] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_W0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_UVSW_EMIT] = {
      .str = "uvsw.emit",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_UVSW_ENDTASK] = {
      .str = "uvsw.endtask",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_END),
      .mod_map = {
         [PCO_OP_MOD_END] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_UVSW_EMIT_ENDTASK] = {
      .str = "uvsw.emit.endtask",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_END),
      .mod_map = {
         [PCO_OP_MOD_END] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_UVSW_WRITE_EMIT_ENDTASK] = {
      .str = "uvsw.write.emit.endtask",
      .num_dests = 0,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_END),
      .mod_map = {
         [PCO_OP_MOD_END] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_W0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FITR] = {
      .str = "fitr",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_END] = 2,
         [PCO_OP_MOD_ITR_MODE] = 3,
         [PCO_OP_MOD_SAT] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 4,
      },
      .src_intrn_map = {
         [1] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FITRP] = {
      .str = "fitrp",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_END] = 2,
         [PCO_OP_MOD_ITR_MODE] = 3,
         [PCO_OP_MOD_SAT] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 4,
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_S2),
            [3] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_LD] = {
      .str = "ld",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_LD),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_MCU_CACHE_MODE_LD] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 4,
      },
      .src_intrn_map = {
         [2] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_IMM),
            [2] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_LD_REGBL] = {
      .str = "ld.regbl",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_LD),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_MCU_CACHE_MODE_LD] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 4,
      },
      .src_intrn_map = {
         [2] = 1,
         [1] = 2,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [2] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ST] = {
      .str = "st",
      .num_dests = 0,
      .num_srcs = 6,
      .mods = (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_IDF),
      .mod_map = {
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 1,
         [PCO_OP_MOD_IDF] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [1] = (1U << PCO_REF_MAP_IMM),
            [2] = (1U << PCO_REF_MAP_DRC),
            [3] = (1U << PCO_REF_MAP_IMM),
            [4] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [5] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5) | (1U << PCO_REF_MAP__),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ST_REGBL] = {
      .str = "st.regbl",
      .num_dests = 0,
      .num_srcs = 6,
      .mods = (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_IDF),
      .mod_map = {
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 1,
         [PCO_OP_MOD_IDF] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [1] = (1U << PCO_REF_MAP_IMM),
            [2] = (1U << PCO_REF_MAP_DRC),
            [3] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [4] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [5] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5) | (1U << PCO_REF_MAP__),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ST_TILED] = {
      .str = "st.tiled",
      .num_dests = 0,
      .num_srcs = 6,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [4] = 4,
         [5] = 5,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [1] = (1U << PCO_REF_MAP_IMM),
            [2] = (1U << PCO_REF_MAP_DRC),
            [3] = (1U << PCO_REF_MAP_IMM),
            [4] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
            [5] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5) | (1U << PCO_REF_MAP__),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_IDF] = {
      .str = "idf",
      .num_dests = 0,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ATOMIC] = {
      .str = "atomic",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM_OP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM_OP] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 4,
      },
      .src_intrn_map = {
         [1] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0) | (1U << PCO_REF_MAP_S1) | (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP_S4) | (1U << PCO_REF_MAP_S5),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SMP] = {
      .str = "smp",
      .num_dests = 1,
      .num_srcs = 6,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_DIM) | (1ULL << PCO_OP_MOD_PROJ) | (1ULL << PCO_OP_MOD_FCNORM) | (1ULL << PCO_OP_MOD_NNCOORDS) | (1ULL << PCO_OP_MOD_LOD_MODE) | (1ULL << PCO_OP_MOD_PPLOD) | (1ULL << PCO_OP_MOD_TAO) | (1ULL << PCO_OP_MOD_SOO) | (1ULL << PCO_OP_MOD_SNO) | (1ULL << PCO_OP_MOD_SB_MODE) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_LD) | (1ULL << PCO_OP_MOD_ARRAY) | (1ULL << PCO_OP_MOD_INTEGER) | (1ULL << PCO_OP_MOD_SCHEDSWAP) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_DIM] = 5,
         [PCO_OP_MOD_PROJ] = 6,
         [PCO_OP_MOD_FCNORM] = 7,
         [PCO_OP_MOD_NNCOORDS] = 8,
         [PCO_OP_MOD_LOD_MODE] = 9,
         [PCO_OP_MOD_PPLOD] = 10,
         [PCO_OP_MOD_TAO] = 11,
         [PCO_OP_MOD_SOO] = 12,
         [PCO_OP_MOD_SNO] = 13,
         [PCO_OP_MOD_SB_MODE] = 14,
         [PCO_OP_MOD_MCU_CACHE_MODE_LD] = 15,
         [PCO_OP_MOD_ARRAY] = 16,
         [PCO_OP_MOD_INTEGER] = 17,
         [PCO_OP_MOD_SCHEDSWAP] = 18,
         [PCO_OP_MOD_F16] = 19,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
         [0] = 5,
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 2,
         [3] = 3,
         [4] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S4),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_S1),
            [3] = (1U << PCO_REF_MAP_S2),
            [4] = (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP__),
            [5] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SMP_WRT] = {
      .str = "smp.wrt",
      .num_dests = 0,
      .num_srcs = 6,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_DIM) | (1ULL << PCO_OP_MOD_PROJ) | (1ULL << PCO_OP_MOD_FCNORM) | (1ULL << PCO_OP_MOD_NNCOORDS) | (1ULL << PCO_OP_MOD_LOD_MODE) | (1ULL << PCO_OP_MOD_PPLOD) | (1ULL << PCO_OP_MOD_TAO) | (1ULL << PCO_OP_MOD_SOO) | (1ULL << PCO_OP_MOD_SNO) | (1ULL << PCO_OP_MOD_SB_MODE) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_ARRAY) | (1ULL << PCO_OP_MOD_INTEGER) | (1ULL << PCO_OP_MOD_SCHEDSWAP) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_DIM] = 5,
         [PCO_OP_MOD_PROJ] = 6,
         [PCO_OP_MOD_FCNORM] = 7,
         [PCO_OP_MOD_NNCOORDS] = 8,
         [PCO_OP_MOD_LOD_MODE] = 9,
         [PCO_OP_MOD_PPLOD] = 10,
         [PCO_OP_MOD_TAO] = 11,
         [PCO_OP_MOD_SOO] = 12,
         [PCO_OP_MOD_SNO] = 13,
         [PCO_OP_MOD_SB_MODE] = 14,
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 15,
         [PCO_OP_MOD_ARRAY] = 16,
         [PCO_OP_MOD_INTEGER] = 17,
         [PCO_OP_MOD_SCHEDSWAP] = 18,
         [PCO_OP_MOD_F16] = 19,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 2,
         [3] = 3,
         [4] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_S1),
            [3] = (1U << PCO_REF_MAP_S2),
            [4] = (1U << PCO_REF_MAP_S3) | (1U << PCO_REF_MAP__),
            [5] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ALPHATST] = {
      .str = "alphatst",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 2,
         [3] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_P0) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_S1),
            [3] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_ALPHAF] = {
      .str = "alphaf",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 2,
         [3] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_P0) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_S0),
            [2] = (1U << PCO_REF_MAP_S1),
            [3] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_DEPTHF] = {
      .str = "depthf",
      .num_dests = 0,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_W0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SAVMSK] = {
      .str = "savmsk",
      .num_dests = 2,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_SAVMSK_MODE),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_SAVMSK_MODE] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_W0) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_W1) | (1U << PCO_REF_MAP__),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_EMITPIX] = {
      .str = "emitpix",
      .num_dests = 0,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_FREEP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_FREEP] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SETL] = {
      .str = "setl",
      .num_dests = 0,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_BACKEND] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_BACKEND] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MOVI32] = {
      .str = "movi32",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_CBS] = {
      .str = "cbs",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_FT2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_FTB] = {
      .str = "ftb",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S2) | (1U << PCO_REF_MAP_FT2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_REV] = {
      .str = "rev",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SHUFFLE] = {
      .str = "shuffle",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 2,
         [0] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S2),
            [1] = (1U << PCO_REF_MAP_S1),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_LOGICAL] = {
      .str = "logical",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_LOGIOP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_LOGIOP] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 3,
         [3] = 4,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT4),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_1] = {
            [0] = (1U << PCO_REF_MAP_FT1) | (1U << PCO_REF_MAP__),
            [1] = (1U << PCO_REF_MAP_FT2),
            [2] = (1U << PCO_REF_MAP_FT1_INVERT) | (1U << PCO_REF_MAP__),
            [3] = (1U << PCO_REF_MAP_S3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SHIFT] = {
      .str = "shift",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_SHIFTOP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_SHIFTOP] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 2,
         [1] = 5,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_2] = {
            [0] = (1U << PCO_REF_MAP_FT5),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_2] = {
            [0] = (1U << PCO_REF_MAP_FT4),
            [1] = (1U << PCO_REF_MAP_S4),
            [2] = (1U << PCO_REF_MAP__) | (1U << PCO_REF_MAP_FT0) | (1U << PCO_REF_MAP_FT3),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_COPYSIGN] = {
      .str = "copysign",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_IBFE] = {
      .str = "ibfe",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [2] = 1,
         [1] = 2,
         [0] = 3,
         [1] = 5,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_UBFE] = {
      .str = "ubfe",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [2] = 1,
         [1] = 2,
         [0] = 3,
         [1] = 5,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_BFI] = {
      .str = "bfi",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [3] = 1,
         [2] = 2,
         [1] = 3,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_BBYP0BM] = {
      .str = "bbyp0bm",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
            [1] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_BBYP0BM_IMM32] = {
      .str = "bbyp0bm_imm32",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
            [1] = (1U << PCO_REF_MAP_FT1),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_BBYP0S1] = {
      .str = "bbyp0s1",
      .num_dests = 2,
      .num_srcs = 1,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT2),
            [1] = (1U << PCO_REF_MAP_FT3),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MSK_BBYP0S1] = {
      .str = "msk_bbyp0s1",
      .num_dests = 3,
      .num_srcs = 3,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
            [1] = (1U << PCO_REF_MAP_FT1),
            [2] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MSK_LSL] = {
      .str = "msk_lsl",
      .num_dests = 3,
      .num_srcs = 3,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW_DIRECT,
      .has_target_cf_node = false,
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_FT0),
            [1] = (1U << PCO_REF_MAP_FT1),
            [2] = (1U << PCO_REF_MAP_FT2),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_0] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S1),
            [2] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_WOP] = {
      .str = "wop",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_WDF] = {
      .str = "wdf",
      .num_dests = 0,
      .num_srcs = 1,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_DRC),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_NOP] = {
      .str = "nop",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_END] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_DITR] = {
      .str = "ditr",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT) | (1ULL << PCO_OP_MOD_SCHED) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_ITR_MODE] = 2,
         [PCO_OP_MOD_SAT] = 3,
         [PCO_OP_MOD_SCHED] = 4,
         [PCO_OP_MOD_F16] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_TEMP),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_COEFF),
            [2] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_DITRP] = {
      .str = "ditrp",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT) | (1ULL << PCO_OP_MOD_SCHED) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_ITR_MODE] = 2,
         [PCO_OP_MOD_SAT] = 3,
         [PCO_OP_MOD_SCHED] = 4,
         [PCO_OP_MOD_F16] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_TEMP),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_DRC),
            [1] = (1U << PCO_REF_MAP_COEFF),
            [2] = (1U << PCO_REF_MAP_COEFF),
            [3] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_DITRP_WRITE] = {
      .str = "ditrp.write",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT) | (1ULL << PCO_OP_MOD_SCHED) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_ITR_MODE] = 2,
         [PCO_OP_MOD_SAT] = 3,
         [PCO_OP_MOD_SCHED] = 4,
         [PCO_OP_MOD_F16] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_DITRP_READ] = {
      .str = "ditrp.read",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_ITR_MODE) | (1ULL << PCO_OP_MOD_SAT) | (1ULL << PCO_OP_MOD_SCHED) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_ITR_MODE] = 2,
         [PCO_OP_MOD_SAT] = 3,
         [PCO_OP_MOD_SCHED] = 4,
         [PCO_OP_MOD_F16] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_CNDST] = {
      .str = "cndst",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_CND] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_PE),
            [1] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_CNDEF] = {
      .str = "cndef",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_CND] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_PE),
            [1] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_CNDSM] = {
      .str = "cndsm",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_CND] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_PE),
            [1] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_S2),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_CNDLT] = {
      .str = "cndlt",
      .num_dests = 3,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_CND] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_PE),
            [1] = (1U << PCO_REF_MAP_W0),
            [2] = (1U << PCO_REF_MAP_P0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_CNDEND] = {
      .str = "cndend",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_PE),
            [1] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_S0),
            [1] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_BR] = {
      .str = "br",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_BRANCH_CND) | (1ULL << PCO_OP_MOD_LINK),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_BRANCH_CND] = 2,
         [PCO_OP_MOD_LINK] = 3,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = true,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_BR_NEXT] = {
      .str = "br.next",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_BR_SKIP_NEXT] = {
      .str = "br.skip_next",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_BRANCH_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_BRANCH_CND] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_MUTEX] = {
      .str = "mutex",
      .num_dests = 0,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_MUTEX_OP),
      .mod_map = {
         [PCO_OP_MOD_MUTEX_OP] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_IMM),
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SAVL] = {
      .str = "savl",
      .num_dests = 1,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
      .grp_dest_maps = {
         [PCO_OP_PHASE_CTRL] = {
            [0] = (1U << PCO_REF_MAP_W0),
         },
      },
      .grp_src_maps = {
         [PCO_OP_PHASE_CTRL] = {
         },
      },
#endif /* NDEBUG */
   },
   [PCO_OP_SCMP] = {
      .str = "scmp",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_OP_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_OP_MAIN] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_BCMP] = {
      .str = "bcmp",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_OP_MAIN) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_OP_MAIN] = 6,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 7,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_BCSEL] = {
      .str = "bcsel",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = 0,
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [0] = 2,
         [2] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_CSEL] = {
      .str = "csel",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_OP_MAIN) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_OP_MAIN] = 6,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 7,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = 0,
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [2] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [0] = 2,
         [2] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_PSEL] = {
      .str = "psel",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 4,
         [2] = 5,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_PSEL_TRIG] = {
      .str = "psel_trig",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 4,
         [2] = 5,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FSIGN] = {
      .str = "fsign",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_ISIGN] = {
      .str = "isign",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FCEIL] = {
      .str = "fceil",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MIN] = {
      .str = "min",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MAX] = {
      .str = "max",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_IADD32] = {
      .str = "iadd32",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_S] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_IMUL32] = {
      .str = "imul32",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_S] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 2,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_UADD_CARRY] = {
      .str = "uadd_carry",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_UADD_SAT] = {
      .str = "uadd_sat",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
         [2] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_TSTZ] = {
      .str = "tstz",
      .num_dests = 2,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ELEM),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_ST32] = {
      .str = "st32",
      .num_dests = 0,
      .num_srcs = 5,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_IDF),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 5,
         [PCO_OP_MOD_IDF] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [3] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_ST32_REGBL] = {
      .str = "st32.regbl",
      .num_dests = 0,
      .num_srcs = 5,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_IDF),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 5,
         [PCO_OP_MOD_IDF] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [2] = 3,
         [3] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_SPILL] = {
      .str = "spill",
      .num_dests = 1,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_UNSPILL] = {
      .str = "unspill",
      .num_dests = 1,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_IADD32_ATOMIC] = {
      .str = "iadd32.atomic",
      .num_dests = 2,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_S),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_S] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 3,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_XCHG_ATOMIC] = {
      .str = "xchg.atomic",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_CMPXCHG_ATOMIC] = {
      .str = "cmpxchg.atomic",
      .num_dests = 2,
      .num_srcs = 3,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [2] = 2,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MIN_ATOMIC] = {
      .str = "min.atomic",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MAX_ATOMIC] = {
      .str = "max.atomic",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_TST_TYPE_MAIN),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_TST_TYPE_MAIN] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [1] = 1,
         [0] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_LOGICAL_ATOMIC] = {
      .str = "logical.atomic",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_LOGIOP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_LOGIOP] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 3,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FLUSH_P0] = {
      .str = "flush.p0",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_END] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MBYP2] = {
      .str = "mbyp2",
      .num_dests = 2,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_HW,
      .has_target_cf_node = false,
      .dest_intrn_map = {
      },
      .src_intrn_map = {
         [0] = 1,
         [1] = 4,
      },
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FNEG] = {
      .str = "fneg",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FABS] = {
      .str = "fabs",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FFLR] = {
      .str = "fflr",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MOV] = {
      .str = "mov",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_MOV_OFFSET] = {
      .str = "mov.offset",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_OFFSET_SD),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_RPT] = 5,
         [PCO_OP_MOD_OFFSET_SD] = 6,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_VEC] = {
      .str = "vec",
      .num_dests = 1,
      .num_srcs = -1,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_COMP] = {
      .str = "comp",
      .num_dests = 1,
      .num_srcs = 2,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FENCE] = {
      .str = "fence",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_FLUSH_DMA] = {
      .str = "flush.dma",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = 0,
      .mod_map = {
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_OP_ATOMIC_OFFSET] = {
      .str = "op.atomic.offset",
      .num_dests = 2,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_RPT) | (1ULL << PCO_OP_MOD_ATOM_OP),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_RPT] = 4,
         [PCO_OP_MOD_ATOM_OP] = 5,
      },
      .dest_mods = {
      },
      .src_mods = {
         [0] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
         [1] = (1ULL << PCO_REF_MOD_ABS) | (1ULL << PCO_REF_MOD_NEG),
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_DYNIDX] = {
      .str = "dynidx",
      .num_dests = 2,
      .num_srcs = 4,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_RPT),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_RPT] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_SMP_DYNIDX] = {
      .str = "smp.dynidx",
      .num_dests = 1,
      .num_srcs = 10,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_DIM) | (1ULL << PCO_OP_MOD_PROJ) | (1ULL << PCO_OP_MOD_FCNORM) | (1ULL << PCO_OP_MOD_NNCOORDS) | (1ULL << PCO_OP_MOD_LOD_MODE) | (1ULL << PCO_OP_MOD_PPLOD) | (1ULL << PCO_OP_MOD_TAO) | (1ULL << PCO_OP_MOD_SOO) | (1ULL << PCO_OP_MOD_SNO) | (1ULL << PCO_OP_MOD_SB_MODE) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_LD) | (1ULL << PCO_OP_MOD_ARRAY) | (1ULL << PCO_OP_MOD_INTEGER) | (1ULL << PCO_OP_MOD_SCHEDSWAP) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_DIM] = 5,
         [PCO_OP_MOD_PROJ] = 6,
         [PCO_OP_MOD_FCNORM] = 7,
         [PCO_OP_MOD_NNCOORDS] = 8,
         [PCO_OP_MOD_LOD_MODE] = 9,
         [PCO_OP_MOD_PPLOD] = 10,
         [PCO_OP_MOD_TAO] = 11,
         [PCO_OP_MOD_SOO] = 12,
         [PCO_OP_MOD_SNO] = 13,
         [PCO_OP_MOD_SB_MODE] = 14,
         [PCO_OP_MOD_MCU_CACHE_MODE_LD] = 15,
         [PCO_OP_MOD_ARRAY] = 16,
         [PCO_OP_MOD_INTEGER] = 17,
         [PCO_OP_MOD_SCHEDSWAP] = 18,
         [PCO_OP_MOD_F16] = 19,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_SMP_WRT_DYNIDX] = {
      .str = "smp.wrt.dynidx",
      .num_dests = 0,
      .num_srcs = 10,
      .mods = (1ULL << PCO_OP_MOD_OLCHK) | (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_END) | (1ULL << PCO_OP_MOD_ATOM) | (1ULL << PCO_OP_MOD_DIM) | (1ULL << PCO_OP_MOD_PROJ) | (1ULL << PCO_OP_MOD_FCNORM) | (1ULL << PCO_OP_MOD_NNCOORDS) | (1ULL << PCO_OP_MOD_LOD_MODE) | (1ULL << PCO_OP_MOD_PPLOD) | (1ULL << PCO_OP_MOD_TAO) | (1ULL << PCO_OP_MOD_SOO) | (1ULL << PCO_OP_MOD_SNO) | (1ULL << PCO_OP_MOD_SB_MODE) | (1ULL << PCO_OP_MOD_MCU_CACHE_MODE_ST) | (1ULL << PCO_OP_MOD_ARRAY) | (1ULL << PCO_OP_MOD_INTEGER) | (1ULL << PCO_OP_MOD_SCHEDSWAP) | (1ULL << PCO_OP_MOD_F16),
      .mod_map = {
         [PCO_OP_MOD_OLCHK] = 1,
         [PCO_OP_MOD_EXEC_CND] = 2,
         [PCO_OP_MOD_END] = 3,
         [PCO_OP_MOD_ATOM] = 4,
         [PCO_OP_MOD_DIM] = 5,
         [PCO_OP_MOD_PROJ] = 6,
         [PCO_OP_MOD_FCNORM] = 7,
         [PCO_OP_MOD_NNCOORDS] = 8,
         [PCO_OP_MOD_LOD_MODE] = 9,
         [PCO_OP_MOD_PPLOD] = 10,
         [PCO_OP_MOD_TAO] = 11,
         [PCO_OP_MOD_SOO] = 12,
         [PCO_OP_MOD_SNO] = 13,
         [PCO_OP_MOD_SB_MODE] = 14,
         [PCO_OP_MOD_MCU_CACHE_MODE_ST] = 15,
         [PCO_OP_MOD_ARRAY] = 16,
         [PCO_OP_MOD_INTEGER] = 17,
         [PCO_OP_MOD_SCHEDSWAP] = 18,
         [PCO_OP_MOD_F16] = 19,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_BREAK] = {
      .str = "break",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_CONTINUE] = {
      .str = "continue",
      .num_dests = 0,
      .num_srcs = 0,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
   [PCO_OP_VOTE] = {
      .str = "vote",
      .num_dests = 1,
      .num_srcs = 1,
      .mods = (1ULL << PCO_OP_MOD_EXEC_CND) | (1ULL << PCO_OP_MOD_VOTE_OP),
      .mod_map = {
         [PCO_OP_MOD_EXEC_CND] = 1,
         [PCO_OP_MOD_VOTE_OP] = 2,
      },
      .dest_mods = {
      },
      .src_mods = {
      },
      .type = PCO_OP_TYPE_PSEUDO,
      .has_target_cf_node = false,
#ifndef NDEBUG
#endif /* NDEBUG */
   },
};

const struct pco_op_mod_info pco_op_mod_info[_PCO_OP_MOD_COUNT] = {
   [PCO_OP_MOD_EXEC_CND] = {
      .print_early = true,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_EXEC_CND_E1_ZX] = "",
         [PCO_EXEC_CND_E1_Z1] = "if(p0)",
         [PCO_EXEC_CND_EX_ZX] = "(ignorepe)",
         [PCO_EXEC_CND_E1_Z0] = "if(!p0)",
      },
   },
   [PCO_OP_MOD_RPT] = {
      .print_early = true,
      .type = PCO_MOD_TYPE_UINT,
      .str = "rpt",
      .nzdefault = 1,
   },
   [PCO_OP_MOD_SAT] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "sat",
   },
   [PCO_OP_MOD_LP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "lp",
   },
   [PCO_OP_MOD_SCALE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "scale",
   },
   [PCO_OP_MOD_ROUNDZERO] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "roundzero",
   },
   [PCO_OP_MOD_S] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "s",
   },
   [PCO_OP_MOD_TST_TYPE_MAIN] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_TST_TYPE_MAIN_F32] = "f32",
         [PCO_TST_TYPE_MAIN_U16] = "u16",
         [PCO_TST_TYPE_MAIN_S16] = "s16",
         [PCO_TST_TYPE_MAIN_U8] = "u8",
         [PCO_TST_TYPE_MAIN_S8] = "s8",
         [PCO_TST_TYPE_MAIN_U32] = "u32",
         [PCO_TST_TYPE_MAIN_S32] = "s32",
      },
   },
   [PCO_OP_MOD_TST_OP_MAIN] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_TST_OP_MAIN_ZERO] = "z",
         [PCO_TST_OP_MAIN_GZERO] = "gz",
         [PCO_TST_OP_MAIN_GEZERO] = "gez",
         [PCO_TST_OP_MAIN_CARRY] = "c",
         [PCO_TST_OP_MAIN_EQUAL] = "e",
         [PCO_TST_OP_MAIN_GREATER] = "g",
         [PCO_TST_OP_MAIN_GEQUAL] = "ge",
         [PCO_TST_OP_MAIN_NOTEQUAL] = "ne",
         [PCO_TST_OP_MAIN_LESS] = "l",
         [PCO_TST_OP_MAIN_LEQUAL] = "le",
      },
   },
   [PCO_OP_MOD_TST_OP_BITWISE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_TST_OP_BITWISE_ZERO] = "z",
         [PCO_TST_OP_BITWISE_NONZERO] = "nz",
      },
   },
   [PCO_OP_MOD_SIGNPOS] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SIGNPOS_TWB] = "twb",
         [PCO_SIGNPOS_PWB] = "pwb",
         [PCO_SIGNPOS_MTB] = "mtb",
         [PCO_SIGNPOS_FTB] = "ftb",
      },
   },
   [PCO_OP_MOD_DIM] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_DIM_1D] = "1d",
         [PCO_DIM_2D] = "2d",
         [PCO_DIM_3D] = "3d",
      },
   },
   [PCO_OP_MOD_PROJ] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "proj",
   },
   [PCO_OP_MOD_FCNORM] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "fcnorm",
   },
   [PCO_OP_MOD_NNCOORDS] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "nncoords",
   },
   [PCO_OP_MOD_LOD_MODE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_LOD_MODE_NORMAL] = "",
         [PCO_LOD_MODE_BIAS] = "bias",
         [PCO_LOD_MODE_REPLACE] = "replace",
         [PCO_LOD_MODE_GRADIENTS] = "gradients",
      },
   },
   [PCO_OP_MOD_PPLOD] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "pplod",
   },
   [PCO_OP_MOD_TAO] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "tao",
   },
   [PCO_OP_MOD_SOO] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "soo",
   },
   [PCO_OP_MOD_SNO] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "sno",
   },
   [PCO_OP_MOD_SB_MODE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SB_MODE_NONE] = "",
         [PCO_SB_MODE_RAWDATA] = "rawdata",
         [PCO_SB_MODE_COEFFS] = "coeffs",
         [PCO_SB_MODE_BOTH] = "both",
      },
   },
   [PCO_OP_MOD_ARRAY] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "array",
   },
   [PCO_OP_MOD_INTEGER] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "integer",
   },
   [PCO_OP_MOD_SCHEDSWAP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SCHEDSWAP_NONE] = "",
         [PCO_SCHEDSWAP_SWAP] = "schedswap",
      },
   },
   [PCO_OP_MOD_F16] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "f16",
   },
   [PCO_OP_MOD_TILED] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "tiled",
   },
   [PCO_OP_MOD_FREEP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "freep",
   },
   [PCO_OP_MOD_SM] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "sm",
   },
   [PCO_OP_MOD_SAVMSK_MODE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SAVMSK_MODE_VM] = "vm",
         [PCO_SAVMSK_MODE_ICM] = "icm",
         [PCO_SAVMSK_MODE_ICMOC] = "icmoc",
         [PCO_SAVMSK_MODE_ICMI] = "icmi",
         [PCO_SAVMSK_MODE_CAXY] = "caxy",
      },
   },
   [PCO_OP_MOD_ATOM_OP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_ATOM_OP_ADD] = "add",
         [PCO_ATOM_OP_SUB] = "sub",
         [PCO_ATOM_OP_XCHG] = "xchg",
         [PCO_ATOM_OP_CMPXCHG] = "cmpxchg",
         [PCO_ATOM_OP_UMIN] = "umin",
         [PCO_ATOM_OP_IMIN] = "imin",
         [PCO_ATOM_OP_UMAX] = "umax",
         [PCO_ATOM_OP_IMAX] = "imax",
         [PCO_ATOM_OP_AND] = "and",
         [PCO_ATOM_OP_OR] = "or",
         [PCO_ATOM_OP_XOR] = "xor",
      },
   },
   [PCO_OP_MOD_MCU_CACHE_MODE_LD] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_MCU_CACHE_MODE_LD_NORMAL] = "",
         [PCO_MCU_CACHE_MODE_LD_BYPASS] = "bypass",
         [PCO_MCU_CACHE_MODE_LD_FORCE_LINE_FILL] = "forcelinefill",
      },
   },
   [PCO_OP_MOD_MCU_CACHE_MODE_ST] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_MCU_CACHE_MODE_ST_WRITE_THROUGH] = "writethrough",
         [PCO_MCU_CACHE_MODE_ST_WRITE_BACK] = "writeback",
         [PCO_MCU_CACHE_MODE_ST_LAZY_WRITE_BACK] = "lazywriteback",
      },
   },
   [PCO_OP_MOD_BRANCH_CND] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_BRANCH_CND_EXEC_COND] = "",
         [PCO_BRANCH_CND_ALLINST] = "allinst",
         [PCO_BRANCH_CND_ANYINST] = "anyinst",
      },
   },
   [PCO_OP_MOD_LINK] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "link",
   },
   [PCO_OP_MOD_PCK_FMT] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_PCK_FMT_U8888] = "u8888",
         [PCO_PCK_FMT_S8888] = "s8888",
         [PCO_PCK_FMT_O8888] = "o8888",
         [PCO_PCK_FMT_U1616] = "u1616",
         [PCO_PCK_FMT_S1616] = "s1616",
         [PCO_PCK_FMT_O1616] = "o1616",
         [PCO_PCK_FMT_U32] = "u32",
         [PCO_PCK_FMT_S32] = "s32",
         [PCO_PCK_FMT_U1010102] = "u1010102",
         [PCO_PCK_FMT_S1010102] = "s1010102",
         [PCO_PCK_FMT_U111110] = "u111110",
         [PCO_PCK_FMT_S111110] = "s111110",
         [PCO_PCK_FMT_F111110] = "f111110",
         [PCO_PCK_FMT_F16F16] = "f16f16",
         [PCO_PCK_FMT_F32] = "f32",
         [PCO_PCK_FMT_COV] = "cov",
         [PCO_PCK_FMT_U565U565] = "u565u565",
         [PCO_PCK_FMT_D24S8] = "d24s8",
         [PCO_PCK_FMT_S8D24] = "s8d24",
         [PCO_PCK_FMT_F32_MASK] = "f32_mask",
         [PCO_PCK_FMT_2F10F10F10] = "2f10f10f10",
         [PCO_PCK_FMT_S8888OGL] = "s8888ogl",
         [PCO_PCK_FMT_S1616OGL] = "s1616ogl",
         [PCO_PCK_FMT_ZERO] = "zero",
         [PCO_PCK_FMT_ONE] = "one",
      },
   },
   [PCO_OP_MOD_PHASE2END] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "phase2end",
   },
   [PCO_OP_MOD_ITR_MODE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_ITR_MODE_PIXEL] = "pixel",
         [PCO_ITR_MODE_SAMPLE] = "sample",
         [PCO_ITR_MODE_CENTROID] = "centroid",
      },
   },
   [PCO_OP_MOD_SCHED] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SCHED_NONE] = "",
         [PCO_SCHED_SWAP] = "schedswap",
         [PCO_SCHED_WDF] = "schedwdf",
      },
   },
   [PCO_OP_MOD_ATOM] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "atom",
   },
   [PCO_OP_MOD_OLCHK] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "olchk",
   },
   [PCO_OP_MOD_END] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "end",
   },
   [PCO_OP_MOD_LOGIOP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_LOGIOP_OR] = "or",
         [PCO_LOGIOP_AND] = "and",
         [PCO_LOGIOP_XOR] = "xor",
         [PCO_LOGIOP_NOR] = "nor",
         [PCO_LOGIOP_NAND] = "nand",
         [PCO_LOGIOP_XNOR] = "xnor",
      },
   },
   [PCO_OP_MOD_SHIFTOP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_SHIFTOP_LSL] = "lsl",
         [PCO_SHIFTOP_SHR] = "shr",
         [PCO_SHIFTOP_ROL] = "rol",
         [PCO_SHIFTOP_CPS] = "cps",
         [PCO_SHIFTOP_ASR_TWB] = "asr_twb",
         [PCO_SHIFTOP_ASR_PWB] = "asr_pwb",
         [PCO_SHIFTOP_ASR_MTB] = "asr_mtb",
         [PCO_SHIFTOP_ASR_FTB] = "asr_ftb",
      },
   },
   [PCO_OP_MOD_CND] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_CND_ALWAYS] = "if(1)",
         [PCO_CND_P0_TRUE] = "if(p0)",
         [PCO_CND_NEVER] = "if(0)",
         [PCO_CND_P0_FALSE] = "if(!p0)",
      },
   },
   [PCO_OP_MOD_FRED_TYPE] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_FRED_TYPE_SIN] = "sin",
         [PCO_FRED_TYPE_COS] = "cos",
      },
   },
   [PCO_OP_MOD_FRED_PART] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_FRED_PART_A] = "a",
         [PCO_FRED_PART_B] = "b",
      },
   },
   [PCO_OP_MOD_MUTEX_OP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_MUTEX_OP_RELEASE] = "release",
         [PCO_MUTEX_OP_RELEASE_SLEEP] = "release.sleep",
         [PCO_MUTEX_OP_RELEASE_WAKEUP] = "release.wakeup",
         [PCO_MUTEX_OP_LOCK] = "lock",
      },
   },
   [PCO_OP_MOD_OFFSET_SD] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_OFFSET_SD_SRC] = "src",
         [PCO_OFFSET_SD_DEST] = "dest",
      },
   },
   [PCO_OP_MOD_IDF] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_BOOL,
      .str = "idf",
   },
   [PCO_OP_MOD_VOTE_OP] = {
      .print_early = false,
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = false,
      .strs = (const char * []){
         [PCO_VOTE_OP_ALL] = "all",
         [PCO_VOTE_OP_ANY] = "any",
      },
   },
};

const struct pco_ref_mod_info pco_ref_mod_info[_PCO_REF_MOD_COUNT] = {
   [PCO_REF_MOD_ONEMINUS] = {
      .type = PCO_MOD_TYPE_BOOL,
      .str = "oneminus",
   },
   [PCO_REF_MOD_CLAMP] = {
      .type = PCO_MOD_TYPE_BOOL,
      .str = "clamp",
   },
   [PCO_REF_MOD_ABS] = {
      .type = PCO_MOD_TYPE_BOOL,
      .str = "abs",
   },
   [PCO_REF_MOD_NEG] = {
      .type = PCO_MOD_TYPE_BOOL,
      .str = "neg",
   },
   [PCO_REF_MOD_FLR] = {
      .type = PCO_MOD_TYPE_BOOL,
      .str = "flr",
   },
   [PCO_REF_MOD_ELEM] = {
      .type = PCO_MOD_TYPE_ENUM,
      .is_bitset = true,
      .strs = (const char * []){
         [PCO_ELEM_E0] = "e0",
         [PCO_ELEM_E1] = "e1",
         [PCO_ELEM_E2] = "e2",
         [PCO_ELEM_E3] = "e3",
      },
   },
};

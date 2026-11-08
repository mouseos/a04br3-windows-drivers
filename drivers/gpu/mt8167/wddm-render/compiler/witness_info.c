/* SPDX-License-Identifier: MIT
 * Read-only PCO opcode/modifier metadata for the allocator-free kernel encoder.
 * The UMD uses the same metadata from its existing PCO library. No compiler
 * optimizer, NIR, allocator, float arithmetic or user-mode runtime is linked
 * into the kernel. Compile with NDEBUG, matching pco_op_info's layout. */
#include "../../../../../third_party/mesa-pco/generated/src/imagination/pco/pco_info.c"

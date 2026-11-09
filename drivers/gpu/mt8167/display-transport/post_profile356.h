/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_POST_PROFILE356_H
#define A04_POST_PROFILE356_H
/* Matched DisplayInitDxe profile: ARGB8888 constant alpha, layer0 no crop.
 * Alpha/control bits in PITCH are retained, never replaced with pitch alone. */
static __inline int a04_post_profile356(unsigned roi,unsigned pitch,unsigned con,unsigned size,unsigned offset){
 return roi==((1280u<<16)|800u)&&pitch==0x00ff0c80u&&con==0x20ffu&&size==roi&&!offset;
}
#endif

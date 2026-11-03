/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Derived from the upstream PowerVR firmware format and the UniXNU-a04br3
 * NativePowerVR MIPS loading sequence. See README.md for provenance. */
#include "firmware.h"
#define PAGE_SIZE 4096u
#define FW_CODE 1u
#define FW_DATA 2u
#define MIPS_PRIVATE_DATA 7u
#define PT_LOAD 1u
#define INFO_SIZE 48u
#define ENTRY_SIZE 24u
#define ELF_HEADER 52u
#define ELF_PROGRAM 32u
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint64_t u64(const uint8_t *p) { return (uint64_t)u32(p) | (uint64_t)u32(p+4)<<32; }
static void zero_bytes(uint8_t *p, size_t n) { size_t i; for (i=0;i<n;++i) p[i]=0; }
static void copy_bytes(uint8_t *dst, const uint8_t *src, size_t n) { size_t i; for (i=0;i<n;++i) dst[i]=src[i]; }
static int within(size_t offset, size_t size, size_t limit) { return offset <= limit && size <= limit-offset; }
static uint32_t rounded(uint32_t n) { return (n + PAGE_SIZE-1u) & ~(PAGE_SIZE-1u); }
static int segment(const struct a04_pvr_fw_plan *p, uint32_t address, uint32_t length,
                   uint32_t *destination)
{
    uint32_t i, matches=0, target=0;
    for (i=0; i<p->section_count; ++i) {
        const struct a04_pvr_section *s=&p->section[i];
        uint32_t delta, base, limit;
        if ((s->type!=FW_CODE && s->type!=FW_DATA) || address<s->base) continue;
        delta=address-s->base;
        if (!within(delta,length,s->size)) continue;
        base=s->type==FW_CODE ? 0 : p->data_offset;
        limit=s->type==FW_CODE ? p->code_size : p->data_size;
        if (!within(s->offset,delta,limit) || !within(s->offset+delta,length,limit)) return 0;
        target=base+s->offset+delta; ++matches;
    }
    if (matches!=1 || !within(target,length,A04_PVR_HEAP_SIZE)) return 0;
    *destination=target;
    return 1;
}
enum a04_pvr_fw_result a04_pvr_fw_plan(const uint8_t *image, size_t length,
        uint64_t actual_bvnc, int allow_mismatch, struct a04_pvr_fw_plan *plan)
{
    struct a04_pvr_fw_plan tmp={0};
    const uint8_t *h; uint32_t i, code=0, data=0, private_count=0, nload=0;
    size_t info, payload_limit;
    if (!image || !plan || length<PAGE_SIZE || length%PAGE_SIZE) return A04_PVR_FW_INVALID;
    info=length-PAGE_SIZE; h=image+info;
    if (u32(h)!=3 || u32(h+4)!=INFO_SIZE || u32(h+12)!=ENTRY_SIZE ||
        u32(h+8)>A04_PVR_MAX_LAYOUT || u32(h+24)!=PAGE_SIZE || !(u32(h+28)&1u) ||
        !within(INFO_SIZE,(size_t)u32(h+8)*ENTRY_SIZE,PAGE_SIZE) ||
        u32(h+40)>info) return A04_PVR_FW_INVALID;
    payload_limit=info-u32(h+40);
    tmp.firmware_bvnc=u64(h+16); tmp.section_count=u32(h+8);
    if (tmp.firmware_bvnc!=actual_bvnc && !allow_mismatch) return A04_PVR_FW_INCOMPATIBLE;
    for (i=0; i<tmp.section_count; ++i) {
        const uint8_t *e=h+INFO_SIZE+i*ENTRY_SIZE; struct a04_pvr_section *s=&tmp.section[i];
        s->id=u32(e); s->type=u32(e+4); s->base=u32(e+8); s->max_size=u32(e+12); s->size=u32(e+16); s->offset=u32(e+20);
        if (s->size>s->max_size || !within(s->offset,s->size,A04_PVR_HEAP_SIZE) ||
            (uint64_t)s->base+s->size>UINT64_C(0x100000000)) return A04_PVR_FW_LAYOUT;
        if (s->type==FW_CODE && code<s->offset+s->size) code=s->offset+s->size;
        if (s->type==FW_DATA && data<s->offset+s->size) data=s->offset+s->size;
        if (s->id==MIPS_PRIVATE_DATA) {
            if (s->type!=FW_DATA) return A04_PVR_FW_LAYOUT;
            tmp.data_offset=s->base&(A04_PVR_HEAP_SIZE-1u); ++private_count;
        }
    }
    if (private_count!=1 || !code || !data) return A04_PVR_FW_LAYOUT;
    tmp.code_size=rounded(code); tmp.data_size=rounded(data);
    if (tmp.code_size>tmp.data_offset || !within(tmp.data_offset,tmp.data_size,A04_PVR_HEAP_SIZE)) return A04_PVR_FW_LAYOUT;
    if (payload_limit<ELF_HEADER || image[0]!=0x7f || image[1]!='E' || image[2]!='L' || image[3]!='F' ||
        image[4]!=1 || image[5]!=1 || image[6]!=1 ||
        u16(image+18)!=8 || u32(image+20)!=1 || u16(image+40)!=ELF_HEADER || u16(image+42)!=ELF_PROGRAM)
        return A04_PVR_FW_ELF;
    tmp.program_offset=u32(image+28); tmp.program_size=u16(image+42); tmp.program_count=u16(image+44);
    if (!within(tmp.program_offset,(size_t)tmp.program_size*tmp.program_count,payload_limit)) return A04_PVR_FW_ELF;
    for (i=0; i<tmp.program_count; ++i) {
        const uint8_t *ph=image+tmp.program_offset+i*tmp.program_size; uint32_t dst, file, mem;
        if (u32(ph)!=PT_LOAD) continue;
        file=u32(ph+16); mem=u32(ph+20);
        if (file>mem || !within(u32(ph+4),file,payload_limit) || !segment(&tmp,u32(ph+8),mem,&dst)) return A04_PVR_FW_ELF;
        ++nload;
    }
    if (!nload) return A04_PVR_FW_ELF;
    *plan=tmp; return A04_PVR_FW_OK;
}
enum a04_pvr_fw_result a04_pvr_fw_load(const uint8_t *image, size_t length,
        uint64_t actual_bvnc, int allow_mismatch, uint8_t *heap, size_t heap_size,
        struct a04_pvr_fw_plan *plan)
{
    struct a04_pvr_fw_plan tmp; enum a04_pvr_fw_result r; uint32_t i;
    if (!heap || !plan || heap_size<A04_PVR_HEAP_SIZE) return A04_PVR_FW_OUTPUT;
    /* Prevent an overlapping input/output from corrupting later ELF segments. */
    if ((uintptr_t)heap <= (uintptr_t)image ? (uintptr_t)image-(uintptr_t)heap<A04_PVR_HEAP_SIZE
        : (uintptr_t)heap-(uintptr_t)image<length) return A04_PVR_FW_OUTPUT;
    r=a04_pvr_fw_plan(image,length,actual_bvnc,allow_mismatch,&tmp);
    if (r!=A04_PVR_FW_OK) return r;
    zero_bytes(heap,A04_PVR_HEAP_SIZE);
    for (i=0; i<tmp.program_count; ++i) {
        const uint8_t *ph=image+tmp.program_offset+i*tmp.program_size; uint32_t dst;
        if (u32(ph)!=PT_LOAD) continue;
        if (!segment(&tmp,u32(ph+8),u32(ph+20),&dst)) return A04_PVR_FW_ELF;
        copy_bytes(heap+dst,image+u32(ph+4),u32(ph+16));
    }
    *plan=tmp; return A04_PVR_FW_OK;
}

/* Linux pvr_rogue_mips.h: 4096 4 KiB PTEs, four 4 KiB page-table
 * pages, 32-bit physical PFN, uncached, dirty, valid and global. */
#define MIPS_EXCEPTION_CODE 5u
#define MIPS_BOOT_CODE 6u
#define MIPS_BOOT_DATA 8u
#define MIPS_STACK 9u
#define MIPS_TABLE_BYTES (4u * PAGE_SIZE)
#define MIPS_BOOT_BYTES 64u
static void put32(uint8_t *p, uint32_t n)
{ p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); p[2]=(uint8_t)(n>>16); p[3]=(uint8_t)(n>>24); }
static void put64(uint8_t *p, uint64_t n) { put32(p,(uint32_t)n); put32(p+4,(uint32_t)(n>>32)); }
static const struct a04_pvr_section *find_section(const struct a04_pvr_fw_plan *p, uint32_t id)
{
    const struct a04_pvr_section *found=0; uint32_t i;
    for (i=0;i<p->section_count;++i) if (p->section[i].id==id) {
        if (found) return 0;
        found=&p->section[i];
    }
    return found;
}
static int section_inside(const struct a04_pvr_section *s, uint32_t type,
                          uint32_t limit, uint32_t min_size)
{
    return s && s->type==type && s->size>=min_size && within(s->offset,s->size,limit);
}
enum a04_pvr_fw_result a04_pvr_mips_prepare(const struct a04_pvr_fw_plan *p,
        uint8_t *heap, size_t heap_size, uint64_t heap_pa,
        uint32_t *table, size_t table_size, uint64_t table_pa,
        struct a04_pvr_mips_addresses *addresses)
{
    const struct a04_pvr_section *boot, *data, *exc, *stack;
    struct a04_pvr_mips_addresses result; uint8_t *config; uint32_t i;
    if (!p || !heap || !table || !addresses || p->section_count>A04_PVR_MAX_LAYOUT ||
        heap_size<A04_PVR_HEAP_SIZE || table_size<MIPS_TABLE_BYTES || ((uintptr_t)table&3u) ||
        (heap_pa&(PAGE_SIZE-1u)) || (table_pa&(PAGE_SIZE-1u)) ||
        heap_pa>UINT64_C(0x100000000)-A04_PVR_HEAP_SIZE ||
        table_pa>UINT64_C(0x100000000)-MIPS_TABLE_BYTES ||
        (heap_pa<table_pa+MIPS_TABLE_BYTES && table_pa<heap_pa+A04_PVR_HEAP_SIZE) ||
        p->code_size>p->data_offset || !within(p->data_offset,p->data_size,A04_PVR_HEAP_SIZE))
        return A04_PVR_FW_OUTPUT;
    /* The CPU mappings must also be distinct. */
    if ((uintptr_t)heap <= (uintptr_t)table ? (uintptr_t)table-(uintptr_t)heap<A04_PVR_HEAP_SIZE
        : (uintptr_t)heap-(uintptr_t)table<MIPS_TABLE_BYTES) return A04_PVR_FW_OUTPUT;
    boot=find_section(p,MIPS_BOOT_CODE); data=find_section(p,MIPS_BOOT_DATA);
    exc=find_section(p,MIPS_EXCEPTION_CODE); stack=find_section(p,MIPS_STACK);
    if (!section_inside(boot,FW_CODE,p->code_size,1) || !section_inside(exc,FW_CODE,p->code_size,1) ||
        !section_inside(data,FW_DATA,p->data_size,MIPS_BOOT_BYTES) ||
        !section_inside(stack,FW_DATA,p->data_size,1)) return A04_PVR_FW_LAYOUT;
    result.boot_code=(uint32_t)(heap_pa+boot->offset);
    result.boot_data=(uint32_t)(heap_pa+p->data_offset+data->offset);
    result.exception_code=(uint32_t)(heap_pa+exc->offset);
    config=heap+p->data_offset+data->offset;
    for (i=0;i<A04_PVR_HEAP_SIZE/PAGE_SIZE;++i) {
        uint64_t pa=heap_pa+(uint64_t)i*PAGE_SIZE;
        table[i]=(uint32_t)((pa>>12)<<6) | 0x17u;
    }
    put64(config,heap_pa+p->data_offset+stack->offset);
    put64(config+8,UINT64_C(0x13000000));
    for (i=0;i<4;++i) put64(config+16+i*8,table_pa+(uint64_t)i*PAGE_SIZE);
    put32(config+48,12); put32(config+52,4); put32(config+56,0); put32(config+60,0);
    *addresses=result; return A04_PVR_FW_OK;
}

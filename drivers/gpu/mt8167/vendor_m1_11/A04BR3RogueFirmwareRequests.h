/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Firmware-to-host ring consumption, based on m1.11 rgxfwutils.c
 * RGXCheckFirmwareCCB. No request is acknowledged until the owning driver
 * reports that it has handled the complete command.
 */
#ifndef A04BR3_ROGUE_FIRMWARE_REQUESTS_H
#define A04BR3_ROGUE_FIRMWARE_REQUESTS_H
#include "A04BR3RogueCommand.h"

enum a04_rgx_request_result {
    A04_REQUEST_EMPTY=0, A04_REQUEST_MORE=1, A04_REQUEST_BLOCKED=2,
    A04_REQUEST_INVALID=3, A04_REQUEST_CORRUPT=4
};

/* Caller serializes the sole consumer against power transitions and removal.
 * Coherent or uncached shared memory must be established by the caller; a
 * cache flush cannot safely preserve both owners' offsets in the same line.
 * acquire/publish order shared memory with the device. handle returns zero
 * only after the typed request has been handled; it must not retain the copy.
 */
struct a04_rgx_request_io {
    void *context;
    void (*acquire)(void *);
    void (*publish)(void *);
    int (*handle)(void *,const RGXFWIF_FWCCB_CMD *);
};

static inline int a04_rgx_firmware_requests(struct a04_rgx_fw_runtime *r,
    const struct a04_rgx_request_io *io,uint32_t budget,uint32_t *handled)
{
    RGXFWIF_CCB_CTL *ctl;RGXFWIF_FWCCB_CMD *ring;
    uint32_t read,write;
    if (!r || !r->init || !io || !io->acquire || !io->publish ||
        !io->handle || !handled || !budget || budget>31) return A04_REQUEST_INVALID;
    *handled=0;
    ctl=(RGXFWIF_CCB_CTL *)a04_rgx_command_memory(r,r->init->psFirmwareCCBCtl.ui32Addr,sizeof(*ctl));
    ring=(RGXFWIF_FWCCB_CMD *)a04_rgx_command_memory(r,r->init->psFirmwareCCB.ui32Addr,32u*sizeof(*ring));
    if (!ctl || ctl!=r->fctl || !ring || ((uintptr_t)ctl&7u) || ((uintptr_t)ring&7u) ||
        a04_rgx_fw_alias(ctl,sizeof(*ctl),ring,32u*sizeof(*ring))) return A04_REQUEST_INVALID;
    while (*handled<budget) {
        RGXFWIF_FWCCB_CMD command;
        io->acquire(io->context);
        read=ctl->ui32ReadOffset;write=ctl->ui32WriteOffset;
        if (ctl->ui32WrapMask!=31 || ctl->ui32CmdSize!=sizeof(*ring) || read>31 || write>31)
            return A04_REQUEST_CORRUPT;
        if (read==write) return A04_REQUEST_EMPTY;
        if (a04_rgx_device_read_words(&command,&ring[read],sizeof(command)))
            return A04_REQUEST_CORRUPT;
        /* Unrecognised magic must remain in the ring for diagnosis. */
        if (((uint32_t)command.eCmdType & 0xffff0000u)!=0x2abc0000u)
            return A04_REQUEST_CORRUPT;
        /* These requests require an object update and a matching KCCB reply
         * (rgxta3d.c). The current callback cannot submit that reply while
         * windows_requests holds command_busy. Leave the request pending
         * until a serialized response transport and typed handlers exist. */
        if (command.eCmdType==RGXFWIF_FWCCB_CMD_ZSBUFFER_BACKING ||
            command.eCmdType==RGXFWIF_FWCCB_CMD_ZSBUFFER_UNBACKING ||
            command.eCmdType==RGXFWIF_FWCCB_CMD_FREELIST_GROW ||
            command.eCmdType==RGXFWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION)
            return A04_REQUEST_BLOCKED;
        if (io->handle(io->context,&command)) return A04_REQUEST_BLOCKED;
        /* An unexpected consumer or damaged header makes acknowledgement unsafe. */
        if (ctl->ui32ReadOffset!=read || ctl->ui32WrapMask!=31 ||
            ctl->ui32CmdSize!=sizeof(*ring)) return A04_REQUEST_CORRUPT;
        io->publish(io->context);
        ctl->ui32ReadOffset=(read+1u)&31u;
        io->publish(io->context);
        ++*handled;
    }
    io->acquire(io->context);
    read=ctl->ui32ReadOffset;write=ctl->ui32WriteOffset;
    if (ctl->ui32WrapMask!=31 || ctl->ui32CmdSize!=sizeof(*ring) || read>31 || write>31)
        return A04_REQUEST_CORRUPT;
    return read==write ? A04_REQUEST_EMPTY : A04_REQUEST_MORE;
}
#endif

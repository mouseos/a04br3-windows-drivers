# GE8300 reset controls (22.40.54.30)

The m1.11 family SOFT_RESET_MASKFULL is a write mask, not a capability mask that must read back on every silicon variant. This fixed profile requires the implemented MMU, GARTEN/MIPS, RASCAL, DUST_A and private SLC controls (0x00000005a8000000). Other family reset bits may read zero. The family write value stays 0x00e7fffffffffc1d; an all-ones bus read and any missing required control still fail.

Primary source: kernel-4.14 commit eb47f42e1f84fe5716ef239620725f82718a620a, drivers/misc/mediatek/gpu/gpu_rgx/m1.11ED5425693. hwdefs/km/configs/rgxconfig_km_22.V.54.30.h defines MIPS, one cluster and one SLC bank. hwdefs/km/rgxdefs_km.h maps one cluster to one DUST. hwdefs/km/rgx_cr_defs_km.h defines the named reset fields. services/server/devices/rgx/rgxstartstop.c RGXStart writes the family mask and reads SOFT_RESET to fence the SOCIF pipeline; it does not require absent family bits to echo.

This change retains every pre-reset idle/read drain, FW OFF acknowledgement, real IRQ count/source drain, IRQ quiet check and asserted reset write. It does not release memory, declare a scheduler completion, or prove NoDMA. The normal parent platform OFF and fresh VerifyOff, retained MDL synchronization and existing retirement proof remain mandatory before hardware-owned allocations can be released. No reset deassert or shared resource write is added.

The existing executable helper model covers the observed implemented-control readback and independently missing required controls. It is a model, not hardware qualification. Runtime verification is required after packaging.

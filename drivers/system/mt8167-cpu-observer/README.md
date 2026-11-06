# MT8167 CPU observation core

This is a read-only interpretation core for future Windows integration. It
accepts two caller-supplied snapshots of the CPU mux, ARMPLL, and MT6392 VPROC
selectors, and optionally the trusted Linux-compatible devinfo word 3. A
mismatch invalidates the entire sample. Equality does not prove an atomic
snapshot: the resource owner must serialize access and exclude competing writers.
`a04_cpu_capture` invokes a read-only, named-field callback twice and fails
closed on a read error or differing values. The owner supplies and validates
the callback and holds the resource lifetime and locks for the entire call.
The function performs no hardware access, locking, clock or voltage change,
thermal policy, or protection. It is not wired to a Windows driver.

The output calls the decoded PLL value configured, not measured CPU frequency;
the decoded VPROC is a programmed selector, not measured voltage. Unknown mux,
PLL power/divider, disabled VPROC, unsupported efuse bin, or absent devinfo are
represented by cleared validity bits. Only the known direct PLL path up to
1300 MHz is decoded. Linux does have a 1500 MHz bin, but this implementation
intentionally does not infer its effective rate or electrical safety.

Linux references: MT8167 DTS cluster0_opp, MT6392 regulator VPROC descriptor,
MT8167 cpufreq and devinfo drivers. Linux supplies devinfo from boot data; do
not guess a physical efuse address. Before any live integration, obtain PnP
resources and synchronization through their owners, verify non-destructive
reads and their power lifetime, and separately validate calibrated thermal
monitoring. This core does not approve DVFS transitions or imply safe operation.

Host test: `cc -std=c11 -Wall -Wextra -Werror -Idrivers/system/mt8167-cpu-observer drivers/system/mt8167-cpu-observer/cpu_observer.c tests/cpu_observer_test.c -o cpu-observer-test && ./cpu-observer-test`.
ARM32 build: `clang --target=thumbv7-pc-windows-msvc -std=c11 -ffreestanding -O2 -Wall -Wextra -Werror -c drivers/system/mt8167-cpu-observer/cpu_observer.c -o cpu-observer.obj`.
The resulting object is not a signed executable or a complete driver.

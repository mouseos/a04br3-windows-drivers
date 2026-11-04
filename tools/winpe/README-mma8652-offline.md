# MMA8652 offline staging experiment

`startnet-stage-mma8652.cmd` is a one-shot WinPE staging operation for the
specific Windows volume recognized by `guard-windows-volume.exe`. It runs only
after the WinPE sensor provider starts and the raw HID diagnostic reads a
sample, package INF/SYS/CAT exactly match the separately bundled reference,
the volume guard accepts C: twice, and a read-only dirty-bit query succeeds
with no flags. A clear dirty bit does not prove filesystem consistency.

Before loading the target SYSTEM hive or running DISM it refuses existing
sensor files, a matching DriverStore directory, an old log or backup directory.
It copies and compares SYSTEM and SOFTWARE, optional COMPONENTS and existing
LOG1/LOG2 files into the new `C:\Windows\Temp\a04br3-mma8652-preinstall`
directory. It refuses unexpected Current control set, existing service or
failed registry enumeration. The hive copies are **not** a full-volume backup,
are on the same volume, and do not cover all DISM side effects. A separately
verified complete volume backup and recovery path are needed for recovery.
No automatic rollback or deletion is attempted; DISM failure can leave
partial changes. Any backup, query or DISM failure requires investigation.

DISM runs without `/ForceUnsigned` and its output is saved to a new log. After
success the script requires exactly one `mma8652.inf_*` DriverStore directory
and compares its INF/SYS/CAT byte-for-byte to the tested reference. A success
marker means only that this offline package was staged and its bytes match; it
is not evidence of a successful Windows boot, signature trust, PnP or sensor
API operation. The self-signed clean-volume helper and package have not been
verified for acceptance by the installed Windows RT policy.

`prepare-driver-probe-wim.py` requires the volume guard, byte comparison tool,
clean-check and both source/reference directories. It checks the exact SHA256
of all five package files against the package recovered from the run3 ESP;
this intentionally rejects rebuilt or re-signed binaries until revalidated.
The evidence image `sensor-result.png` shows PnP started and raw HID values
(-0.01, 0.01, 0.99 g), not Windows installed-OS Sensor API operation.
The clean-check helper only queries FSCTL_IS_VOLUME_DIRTY and never repairs.
`tools/ci/test-mma8652-offline-stage.ps1` runs the batch in a disposable mock
folder with registry, device access and DISM replaced by mocks. No real disk,
filesystem or installed OS is touched by that test.

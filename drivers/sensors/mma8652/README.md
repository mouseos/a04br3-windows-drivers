# MMA8652 Windows RT sensor experiment

The stock DTS places a generic accelerometer on MT8167 I2C1 `0x1100a000` at
`0x1d`. The earlier Linux [hardware report](https://github.com/mouseos/a04br3-linux-drivers/blob/main/docs/accelerometer-bringup.md)
identifies the actual part as MMA8652 by WHO_AM_I `0x4a` and records samples.
That is not a Windows measurement. Board direction 7 maps board axes to
`-sensor Y, -sensor X, -sensor Z`.

On TAB-A04-BR3, a three-CPU Windows RT 8.1 WinPE test with the opt-in I2C1
handoff built from UEFI commit `b8ca32e` loaded the test-signed driver and
started its PnP provider. A HID read returned one stationary sample of
X=-0.01 g, Y=0.01 g and Z=0.99 g. This confirms that particular test path;
axis motion, sustained operation, sleep/resume and installation in the
Windows system are not yet verified. A separate four-CPU WinPE attempt reset
after ExitBootServices before the test could run.

The companion UEFI branch `windows-rt/mma8652-handoff` allocates a per-boot
ACPI NVS page and publishes SSDT `MOU0010` with exclusive I2C1 and handoff
resources. At BeforeExitBootServices it configures clock, GPIO52/53 and I2C1;
only successful setup writes the marker, timing and complement. AML HSTA
checks the marker and clock/pin state. Windows evaluates HSTA, verifies the
page and claims the sole I2C1 resource before accessing its registers. On D0
entry it restores the controller from the handed-off timing and configures the
sensor; on exit it stops and purges the report queue. If shared clock or pins
are lost on resume, HSTA fails and D0 is refused: Windows does not own or
restore the shared registers. I/O errors latch a failure instead of resetting
an unknown bus state. This handshake, including ACPI NVS resource allocation,
cache attributes and ACPI method routing through the HID stack, was exercised
by the single WinPE read above, but is not yet validated across power states.
The marker is an integrity check, not authentication.

No camera ACPI device or other Windows I2C1 owner may be enabled in this
configuration. A future camera must use a common controller owner and lock;
a local lock or Windows resource assignment cannot arbitrate uncooperative
firmware. UEFI stops using I2C1 after its handoff. Existing touch owns the
shared TOP, INFRA and GPIO resources; Windows sensor code never maps them.
The AML methods read them. The SSDT in the UEFI repository is authoritative;
the standalone `platform/a04br3/acpi/Mma8652.asl` is an obsolete proposal and
must not be installed alongside it.

The HID descriptor targets the [Windows 8 sensor HID class](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/sensor-hid-class-driver),
providing input X/Y/Z in hundredths of g and feature state, interval, minimum
interval and absolute sensitivity. It supports no-events, all-events and
threshold-events. HID feature contract, enumeration and sensor-class binding
remain incompletely verified on Windows RT. Because this is an ACPI device with no USB
VID/PID, attributes use zero for both; these are unassigned transport IDs, not
an assertion that the chip has a USB identity. Actual identity is checked via
WHO_AM_I. WinPE accepted these attributes for the single read above; behavior
in the installed Windows system remains unverified.

Run `make -C drivers/sensors/mma8652 test test-hid arm-coff` for simulated
host tests and ARM object compilation. `tools/ci/build-mma8652.ps1` builds an
ARM32 WDK package and `tools/ci/sign-mma8652.ps1` creates a research-only
self-signed package; acceptance outside the test-mode WinPE is not established. Do not treat
host simulation, linking or a signature as evidence of hardware operation.

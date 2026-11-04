# A04BR3 CPU stage snapshots

`python3 tools/cpu/decode-stage.py saved-snapshot.json` decodes saved SEC and
EBS strings from the A04BR3 CPU stage diagnostic. It does not contact a device.
It reports a configured PLL rate only for the ARMPLL mux, enabled PLL and
zero post-divider configuration used by the board SEC code. Other configurations
are unknown. The 26 MHz input is the assumption used by that code. Snapshots
are not atomic, and neither the PLL rate nor equal snapshots establish the
actual CPU frequency, intermediate behavior, voltage, temperature or safety.

Run `python3 -m unittest discover -s tests -p cpu_stage_decode_test.py` for the
host tests. No CPU frequency, voltage, ACPI, or device setting is changed.

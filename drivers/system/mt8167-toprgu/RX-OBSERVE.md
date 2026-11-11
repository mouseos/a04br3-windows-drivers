# TOPRGU bounded RX observation (source candidate)

`A04_TOPRGU_RX_OBSERVE` is **0 by default** and accepts only 0/1. It adds private
WDF context and static source-linked helpers, not an ABI, GUID, IOCTL or EDK2
change. No new GPIO, baud/divisor, FCR, power or watchdog policy writes are added.
The existing command/parser/reset request is unchanged. This is observation,
not a fix for an unresponsive reset or evidence of the current device's RX state.

The source base is 0821853f798407858a1e2995b8f4e7d9d13c3e36. The separately saved
run232 provenance binds that normal-uart source/macros to retained SYS
8caa8d85dc27251f0f00bf5f60e05657e30c6c3ff4140147f121d3467518c554.
That old SYS has not been replaced, recompiled, signed or reverse engineered here.
An observer-enabled SYS does not yet exist from this source task.

## Receive and TX boundaries

ON samples each **existing** LCR read, existing conditional LSR read, and actual
RBR consumption. DLAB still prevents the LSR/RBR accesses; DR still guards RBR;
the 16-byte poll budget, parser and software-reset write remain unchanged.
Parser observation receives only before/after matched counts and the match bit,
not the received byte. No received byte is formatted, stored in observation
context, or transmitted as diagnostic content.

`Trace()` itself polls LSR while transmitting. Therefore observation does NOT call
Trace or add another UART status read. At the end of an eligible poll it reuses
the last RX LCR/LSR sample, and writes **at most one diagnostic THR byte** if DLAB
was clear and THRE was set. There is no extra LCR, LSR or RBR read and no waiting
TX loop. LSR is potentially clear-on-read; OE/DR counters count only the LSR
samples from this poll, not all historical hardware events. Other existing TX
Trace calls can still read/clear LSR, and another UART consumer may consume RX.

The existing `screenLen` Trace guard stays intact. An observation line cannot be
queued or transmitted while a screen packet is pending. While an observation
line is pending, this driver's Trace is suppressed and a new screen IOCTL gets
the existing STATUS_DEVICE_BUSY response. The caller must handle that existing
backpressure result; the packet is not accepted then discarded or interleaved.
At normal cadence a 205-byte line needs up to 205 eligible THRE-ready polls.
If TX stays unready or DLAB stays set, the line can remain pending and suppress
Trace/screen admission; counters and RX polling still run, with no busy-wait or
repeated diagnostic output. This opt-in effect is not concealed as transparent.
It coordinates only this driver's writers; unrelated UART writers can still tear
a record. Partial/interleaved records are not evidence of zero RX.

## Finite budget and counters

- Maximum **24 observation records / 4920 bytes per WDF device instance**.
- Four idle records maximum (first + heartbeat at least 60 seconds apart).
  Twenty records are reserved for late RX/state changes, so idle startup cannot
  consume the budget before a later host command.
- All successive records are queued at least 10 seconds apart. Intermediate
  states may coalesce; counts are cumulative. DLAB observations and LCR change
  counts preserve transient configuration changes even if the last LCR returned
  to its previous value before another record could be queued.
- Counters saturate at UINT32_MAX with a per-counter saturation bitmap. They
  never silently wrap. Budget and counters survive D0 exit/reentry. D0 counter
  saturation also prevents claiming a unique D0 based only on that field.
- A line is a frozen snapshot at queue time, not transmission-completion time.
  A queued old-D0 line retains that old D0 tag across a resume. Sequence numbers
  count queued records; interrupted/reset transmission need not yield a complete
  line. The last quota word shows whether the observation budget is exhausted.
- A matched reset still writes the old software-reset request immediately after
  updating memory counters; no diagnostic TX is awaited. The machine can reset
  before its match counter is emitted. Missing match text does not disprove reset.

Only records with prefix `A04RX1 `, exactly 22 eight-digit uppercase hex integers
separated by one space, and a final newline are complete (205 bytes, no NUL).
The fields, zero based:

| Index | Meaning |
|---|---|
| 0 | queued sequence (1..24) |
| 1 | device-instance D0 entry count |
| 2 | queue time: interrupt-time low32, 100ns units (wraps after 429.4967296 seconds) |
| 3 | last sampled LSR validity: 0/1 |
| 4 | last sampled LCR low8 |
| 5 | last sampled LSR low8; stale/unavailable when field3=0 |
| 6 | eligible running timer/DPC poll entries |
| 7,8 | LCR / LSR sampled-read counts |
| 9,10 | sampled DR / sampled OE counts |
| 11,12 | RBR-consumed byte / parser-call counts |
| 13,14,15 | parser forward-progress / mismatch-or-invalid-state / complete-match counts |
| 16 | parser matched prefix count at queue time |
| 17 | saturation bitmap |
| 18 | record deferrals due to a pending screen packet |
| 19 | (maximum24 <<16) \| (idle records <<8) \| state-change records |
| 20,21 | sampled DLAB-set / LCR-change counts |

Saturation bits: 0 D0, 1 polls, 2 LCR reads, 3 LSR reads, 4 DR, 5 OE,
6 RBR bytes, 7 parser calls, 8 matches, 9 forward progress, 10 mismatch,
11 screen deferrals, 12 DLAB samples, 13 LCR changes.

Valid current-instance observations with progressing polls/LSR reads but DR/RBR/
parser counts zero mean **this poll did not observe/consume RX**. They do not
prove physical RX0, an unplugged wire, or absence of another consumer. RBR and
parser counts increasing with mismatches and no match distinguish consumed input
from parser non-match. DLAB counts/LSR validity identify the no-LSR-access gate.
Bind the new signed SYS, macro/source manifest, main seal and actual boot/UART
window externally before treating any record as current; neither the prefix nor
D0 count alone proves source, boot or hardware success.

## Source/build and host validation

The generic `tools/ci/build-mt8167-toprgu.ps1` accepts explicit environment
`A04_TOPRGU_RX_OBSERVE=1`; missing means 0 and other values are rejected. Keep the
normal-uart CPU1=1/screen=1/thermal=0/deadline=0 tuple explicit in any future build
receipt. The historical `build-toprgu-normal-uart.ps1` byte-reference guard is
unchanged and correctly rejects this changed source; do not relax it or label
this source as its old runtime. A future incremental candidate needs the changed
wdf_toprgu TU plus this private header; reset_command source is unchanged.

`python3 tests/toprgu_rx_observe_test.py --output NEW_PROJECT_LOG_DIR` runs one
combined host batch. Actual poll/Trace/screen admission/parser/formatter functions
execute with fake CPU/MMIO/WDF under ASan/UBSan. It checks read sequences, no-DR,
DLAB/overrun, parser progress/mismatch/match, 16-byte bound, screen exclusion,
counter saturation, late-event reservation and rate/lifetime caps. All five
legal handoff/screen/thermal macro-OFF configurations also have whole-source
preprocessed token equality with the base. WDK compilation, true IRQ concurrency,
actual UART transmission, current SYS replacement and device RX remain untested.

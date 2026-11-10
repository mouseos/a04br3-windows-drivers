# GE8300 early retirement evidence

GPU364 was loaded in run191 (INF 0.0.3.64 and SizeOfImage 299008).
QUERYSEGMENT3 returned success; firmware retirement R36 followed 0.219 seconds
later. The evaluator found only BasicRender/BasicDisplay, and did not reach
Present. Stop/Power/Drain364 records were absent; their APC guard means absence
is unknown. In particular POST release calls StopOwnerSession directly.

365 adds observations without changing DDI acceptance, ownership, MMIO,
capabilities, firmware, UMD or DIS0. The following schema2 40-byte records use
the existing FactoryCheckpoint PASSIVE/special-APC guard and FILETIME.

| Record | Meaning | a,b,c,d |
| --- | --- | --- |
| R80 / R81 | graph retirement entry / return | origin, detail, owner state, job state |
| R82 / R83 | CreateDevice entry / return | flags, session state, 0, 0 |
| R84 / R85 | CreateContext entry / return | flags, node ordinal, engine affinity, private bytes |
| R86 | POST quiesce entry | 0, session state, 0, 0 |
| R87 | POST restore return | 1, 0, 0, 0 |
| R88 | POST StopOwnerSession return | 2, 0, 0, 0 |
| R89 | POST ReleaseMaps return | 3, 0, 0, 0 |

Retirement origins 1..9 are quiesce, cold bind, linear unbind, target unbind,
clear rebuild, discard, vertex selection, draw rebuild, display allocation
drain. Only origin1 has detail=GEO_QUIESCE_STOP/POWER/TDR. All nine source
calls pass through the same observer immediately around the unchanged real
retirement function. A valid R80/R81 pair around a fresh R36 identifies the
retirement caller; it does not by itself explain why Windows requested it.

Records are last-record-only. Match timestamps and actual loaded module to
the captured boot. Missing records do not prove a callback was absent. Status
zero reports a real callback return, not successful rendering. No new forced
registry flush is introduced.

`tools/ci/build-ge8300-retirement365.py` replaces three translation units over
the pinned signed364 package, retains its miniport/palette, checks every other
archive member, and uses Microsoft LINK14.29 and the existing signing cache.
No SDK, PCO compiler, UMD or native dependency rebuild is required. The normal
public source build also includes these sources. Real adapter publication,
Present and DWM remain unverified for365 until a new hardware capture.

/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_SPEAKER_NOMINAL_LATENCY_H
#define MT8167_SPEAKER_NOMINAL_LATENCY_H
/* CT3 mt8167_afe_hardware.fifo_size=0, and soc_pcm_pointer's extra delay
 * remains 0 for the actual AFE/MT8167 codec ops (no delay callback).
 * These are VENDOR-REPORTED NOMINAL values for the existing native 48k stereo
 * S16 DL1 -> internal DAC -> AU_LOL -> MT6392 analog speaker route. They do
 * NOT certify zero physical FIFO/filter/analog latency, measured precision,
 * PCI semantics, HP, capture, BT/MRG or a different rate/format/route.
 * Microsoft permits a rate-dependent best estimate, not a calibration gate.
 * See SPEAKER-NOMINAL1034.md for the limitations and exact source anchors. */
static inline void Mt8167SpeakerNominalHwLatency(KSRTAUDIO_HWLATENCY *p)
{
    p->FifoSize=0; /* Explicit vendor PCM FIFO report, never ring/period bytes. */
    p->ChipsetDelay=0; /* No additional packet-service delay in vendor model. */
    p->CodecDelay=0; /* No vendor codec delay correction reported; not measured. */
}
#endif

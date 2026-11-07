; Integration fragment ONLY. Not an installable INF; no new service or PDO.
; Audio owner merges AddInterface lines into its existing DDInstall.Interfaces
; and these interface/AddReg sections into its own signed INF. Do not deploy
; until provider v2, PortCls slots and Audio FDO callsites are implemented.
[MT6630.Hfp.Interfaces]
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},MT6630SpeakerWave,MT6630.Hfp.Speaker
AddInterface={65e8773e-8f56-11d0-a3b9-00a0c9223196},MT6630SpeakerWave,MT6630.Hfp.Speaker
AddInterface={eb115ffc-10c8-4964-831d-6dcb02e6f23f},MT6630SpeakerWave,MT6630.Hfp.Speaker
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},MT6630SpeakerTopology,MT6630.Hfp.Speaker
AddInterface={dda54a40-1e4c-11d1-a050-405705c10000},MT6630SpeakerTopology,MT6630.Hfp.Speaker
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},MT6630MicWave,MT6630.Hfp.Mic
AddInterface={65e8773d-8f56-11d0-a3b9-00a0c9223196},MT6630MicWave,MT6630.Hfp.Mic
AddInterface={eb115ffc-10c8-4964-831d-6dcb02e6f23f},MT6630MicWave,MT6630.Hfp.Mic
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},MT6630MicTopology,MT6630.Hfp.Mic
AddInterface={dda54a40-1e4c-11d1-a050-405705c10000},MT6630MicTopology,MT6630.Hfp.Mic
[MT6630.Hfp.Speaker]
AddReg=MT6630.Hfp.Speaker.AddReg
[MT6630.Hfp.Mic]
AddReg=MT6630.Hfp.Mic.AddReg
[MT6630.Hfp.Speaker.AddReg]
HKR,,CLSID,,{17cca71b-ecd7-11d0-b908-00a0c9223196}
HKR,,FriendlyName,,"MediaTek MT6630 HFP Speaker"
[MT6630.Hfp.Mic.AddReg]
HKR,,CLSID,,{17cca71b-ecd7-11d0-b908-00a0c9223196}
HKR,,FriendlyName,,"MediaTek MT6630 HFP Microphone"

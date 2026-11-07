; Fixed candidate fragment, NOT a standalone/stageable INF.
; Requires a separately reserved higher-version Audio/parent package and a
; real capture owner. Merge Capture.Interfaces into Install.NT.Interfaces.
[Capture.Interfaces]
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},Mt8167CaptureWaveRT,CaptureWave.Interface
AddInterface={65e8773d-8f56-11d0-a3b9-00a0c9223196},Mt8167CaptureWaveRT,CaptureWave.Interface
AddInterface={6994ad04-93ef-11d0-a3cc-00a0c9223196},Mt8167CaptureTopology,CaptureTopology.Interface
AddInterface={dda54a40-1e4c-11d1-a050-405705c10000},Mt8167CaptureTopology,CaptureTopology.Interface
[CaptureWave.Interface]
AddReg=CaptureWave.AddReg
[CaptureWave.AddReg]
HKR,,CLSID,,{17cca71b-ecd7-11d0-b908-00a0c9223196}
HKR,,FriendlyName,,"MediaTek MT8167 Microphone"
[CaptureTopology.Interface]
AddReg=CaptureTopology.AddReg
[CaptureTopology.AddReg]
HKR,,CLSID,,{17cca71b-ecd7-11d0-b908-00a0c9223196}
HKR,,FriendlyName,,"MediaTek MT8167 Microphone"
HKR,"EP\0","{1DA5D803-D492-4EDD-8C23-E0C0FFEE7F0E},2",,"{DFF21BE1-F70F-11D0-B917-00A0C9223196}"
; Fixed stereo S16/48k PCM KS contract; this is not a physical mic-count claim.
HKR,"EP\0","{E4870E26-3CC5-4CD2-BA46-CA0A9A70ED04},3",0x00000001,41,00,00,00,28,00,00,00,FE,FF,02,00,80,BB,00,00,00,EE,02,00,04,00,10,00,16,00,10,00,03,00,00,00,01,00,00,00,00,00,10,00,80,00,00,AA,00,38,9B,71

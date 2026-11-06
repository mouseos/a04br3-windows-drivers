# MediaTek MT8167 AFE / MT6392 実PCM経路

## 到達点と制約

基点 a016d53 の first-playback は通常音声ではない。SILENT_DMA と
PCM_PATTERN が WaveRT 公開bufferとは別の common buffer に固定toneを生成する。
AudioSrv が書くPCMはそのDMAに届かず、WaveRT subdevice単独でspeaker topologyもなかった。

新しい EnginePcm は既定オフ。48 kHz / 16 bit / 2 channel の PCM と
WAVEFORMATEXTENSIBLE（valid bits 16、FL|FR）を受け入れる。
PortCls が割り当て、kernel側をMmWriteCombinedでmapした同一MDLをclientへ公開し、
保持した単一SG elementのDMA論理アドレスをDL1へ渡す。
parentはMDL全ページのPFNとDMA論理アドレスが連続して一致すること、
実clock/codec leaseが保持されていることを検証する。
HALのbounce bufferや別ページへのコピーを必要とする構成は開始しない。
一回のcache flushを継続的なcoherencyの証拠にしない。
このWindows mapping契約による実装と、実機でPCM更新がDMAへ届く確認は別である。

WaveRT host pin 0 → DAC → analog bridge 1 →
PcRegisterPhysicalConnection → Topology input 0 → speaker output 1。
AU_LOL / MT6392 Class Dの既存Linux由来経路を使い、架空のvolume/mute nodeや
jack検出は追加しない。Topologyのspeaker categoryで内蔵endpointを識別し、
Topology interfaceのEP/0に48 kHz / 16 bit / stereoのOEMFormatを設定する。
event notificationはadvertiseせず、GetPositionでDL1_CURをpollする。

**通常音声の実機完成とは扱わない。** EnginePcmはPAUSE→RUN、
STOP→RUNとstream再openを実装する。PnP ownerが最初のPortCls allocation、
そのallocating portへの参照、同じSG mappingをresetまで保持し、clientは一つずつ
checkout/checkinする。FreeAudioBufferはclient所有権を返すだけでページを解放しない。
再openで最初のallocationより大きい要求は拒否する。DMA drainなしの拡張・置換はしない。

RUN→PAUSEではmute→IRQ同期下DL1停止→IRQ1 mask/ack/readback→旧DPCのremove/flushを行う。
CURを保存し、PAUSE中のpositionを維持する。PAUSE→RUNはLinux RESUMEと同じく
BASE/ENDを保持して再enableする。STOPでは論理positionを0に戻し、次のRUNは
Linux hw_paramsと同じBASE/END設定を同じringに繰り返す。
DL1 enable、IRQ enable、IRQ statusの停止readback、CURの有効範囲、
ソフトウェアIRQ/DPC回収を確認できない場合はpoisonを維持して次のRUNを拒否する。
IRQ faultを再開のために消さず、世代付きDPCは古い世代のpendingを消費しない。
再enableのreadbackも確認する。任意のsleepやCURが止まるまでの推測waitは追加しない。

共有leaseのquarantinedは引き続き「解放禁止」の意味で保持する。
再利用可能な停止状態AUDIO_STOPPED_RETAINEDと、失敗poisonを別に管理する。
これは同じallocationの再利用条件でありfabric DMA drainの証明ではない。
voice/parent D0/pages/SGを解放しない。PnP再開始、suspend/resumeと強制removeの
物理資源維持は未対応。旧fixed-tone profileは従来どおり初回RUN専用。

GetHWLatencyの既存ゼロ値は未校正のplaceholderである。
codec/fabricの実latency、AudioEndpointBuilderの実列挙、AudioSrv接続、
実PCM更新、可聴出力は未確認。
MT6392 hardware trimとLinux cold-start DC profileを引き継ぐが、
AU_LOL専用ADC実測校正を実装済みとは扱わない。

## ARM32のCPU mapping契約（329413aの修正）

以前のMmNonCachedを通常RAMと扱う説明は誤りであり、採用しない。
EnginePcmの公開MDLはもともとCommonBuffer由来ではなく、
PortCls AllocateContiguousPagesForMdlが返す、lockedだがunmappedな物理ページである。
それでもMmNonCachedを指定するとARMでDevice/Strongly-orderedになる可能性があり、
AudioSrvへalignment制約を課すことはできない。

修正版では次を一つの契約として実装する。
1. 最初のkernel aliasをMapAllocatedPages(MmWriteCombined)で作る。
   MmNonCachedへfallbackせず、CommonBufferを別属性でremapすることもしない。
2. CPUでゼロ埋めする前に、各kernel VAにATS1CPRを実行しPARを読む。
   Cortex-A53のAArch32 short/long descriptorの両方を解釈し、
   NormalかつInner/Outer Non-cacheableで、PortClsが示した同じ物理ページかを確認する。
   Device、Strongly-ordered、cached、translation fault、異なる物理ページは拒否。
   未公開mappingはunmapして割当て失敗を返す。retained mappingの再open時にも再確認し、
   属性不一致はpoisonにする。確認前にmemset/memcpy/SIMDでbufferを触らない。
3. AllocateAudioBufferのCacheTypeもMmWriteCombinedに統一する。
   PortClsが同MDLをuserへmapし、標準KSRTAUDIO_BUFFERのCallMemoryBarrierをTRUEとして
   clientへ伝える契約を使用する。kernel/userで異なるcache属性を要求しない。
4. DMA adapterの単一SG mapping、parentのPFN/DMA identity確認、同じMDLのDL1公開を維持。
   AudioSrvが書くページとhardwareの参照先は同じであり、固定tone・shadow・別buffer copyには戻さない。
   初期flushだけをlive PCM整合の根拠にしない。

PARはper-CPU scratchなので、各ページの短い照会だけHIGH_LEVELで実行し、元のPARを復元する。
page table、MAIR、SCTLR、AFEレジスタは変更しない。待機・delay loop・TLB書換えはない。
ATS1CPRは**stage 1**の照会であり、EL2のstage 2やfirmware全体の証明ではない。
WindowsのMM/PortClsが公開DDIどおり同じphysical-page属性をuser aliasへ使うことを前提とする。
user VAはminiportのAllocateAudioBuffer復帰後にPortClsが作るため、
このcallback内でAudioSrvのuser PTEを観測したとは主張しない。
対象OSの実user mappingとEL2の最終属性は実機未観測である。
WCという名前からNormal-NCを推定したり、ARM64の実測をARM32へ流用したりしない。

一次資料:
- [MapAllocatedPages](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iportwavertstream-mapallocatedpages)
  Remarksは、audio stackがalignment要件を表せず、driverは追加alignment制約を課さない
  cache typeを選ぶ必要があると定める。
- [AllocateContiguousPagesForMdl](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iportwavertstream-allocatecontiguouspagesformdl)
  はlocked/unmappedなpagesを返す。HAL common-bufferのcache属性を引き継ぐAPIではない。
- [MmMapLockedPagesSpecifyCache](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-mmmaplockedpagesspecifycache)
  は、既にcache typeがあるphysical pageでは既存属性を使うと定める。
  MapAllocatedPagesはこのAPIと同様のoperationである。
- [MEMORY_CACHING_TYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ne-wdm-_memory_caching_type)
  は同じphysical pageのaliasに異なるcache behaviorを指定してはならないと定める。
- [KSRTAUDIO_BUFFER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-ksrtaudio_buffer)
  はMmWriteCombinedならCallMemoryBarrier=TRUEを指定する。
  user clientは通常のPCM書込み後にこの通知どおりのbarrierを使う。
- [Arm ARM DDI 0406C.d](https://documentation-service.arm.com/static/5f8daeb7f86e16515cdb8c4e)
  B3.14.1/B4.2.4（ATS1CPRのstage-1照会と命令encoding）、
  B4.1.112 pp.B4-1658–1662（PARのF/LPAE/PA/Inner/Outer/ATTR）、
  B4.1.104 Tables B4-7/B4-8（MAIR 0x44のNormal Inner/Outer NC）を使用。
  short PARではInner=000/Outer=00だけを許可し、Inner=001/011を許可しない。
  long PARではATTR=0x44だけを許可する。Supersection結果は採用しない。

## audioだけの増分build

既存のEnginePcm一括build出力（親SYSとparent-manifest.jsonを含む）を指定する。

    powershell -File tools/ci/build-mt8167-first-playback.ps1 -EnginePcm -IntegratedUsb -IntegratedWifi -SkipPeripheralBuild -ReuseParent -OutputDirectory build/mt8167-engine-pcm

この呼出しは親flagsとSYS SHA-256を検査した後、audioの各objectとSYSを更新する。
USB/WiFi SYS、KS tone client、共有親SYSはcompileしない。
署名対象はaudio/mt8167-audio.sysとaudio/mt8167-afe-engine-pcm.inf（2026.11.7.1004）。
v11親のPFN/lease検証ロジックは今回変更していないため一致するEnginePcm親を使える。
古いfixed-tone親や必要flagの異なる親は再利用できない。
PowerShell実行・WDK compile/link・署名・実機は主担当の工程で、この作業では実施していない。

## 主担当向けWDK 8.1単位

この実PCM profileはparent verifierも変えるため、parentとaudioの組が必要。
既存のa016d53診断bundleを成功済みの実PCM版として置換しない。
49d5c65以降のEnginePcm parentのソースは今回変更していない。
一致した既存出力に -ReuseParent を付けると、manifestの親flagsとSYS hashを照合し、
parentを再compileせずaudioだけを更新する。異なるprofileの親は再利用しない。

    powershell -File tools/ci/build-mt8167-first-playback.ps1 -EnginePcm -IntegratedUsb -IntegratedWifi -SkipPeripheralBuild

単一parentのUSB/WiFi機能flagを維持してparent/audioだけをbuildする。
USB/WiFiのSYS、既存KS client、EDK2はbuildしない。
出力既定は build/mt8167-engine-pcm。別profileと同じ出力先を使わない。
署名・CAT・媒体配置・ACPIと実機操作は主担当が行う。
音声INFは mt8167-afe-engine-pcm.inf（2026.11.7.1004）。
既存mt8167-afe-first-playback.infは固定tone診断用のまま。

EnginePcmでは旧KS診断clientを自動起動しない。旧clientは自分でpinをRUN/STOPし、
固定tone判定を行うためAudioSrv経路の証拠にならず、AudioSrvとpinを競合する。
またKSCATEGORY_AUDIOにTopologyも登録するため、旧clientの単一interface前提を満たさない。
通常OSのAudioEndpointBuilder/AudioSrvからPCMを書き、
DMA進行とsample更新、codec状態を対応づけて確認する必要がある。
WinPEでのAudioSrv無しprivate-tone診断は引き続き旧profileを使う。
v11のread-only snapshot IOCTLは実PCM版でも有効である。

## 根拠

- Microsoft: [AllocateAudioBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-allocateaudiobuffer)。
  PortClsによるMDL割当て、clientへの同一buffer公開、RUN時開始。
- Microsoft: [WaveRT miniport](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/developing-a-wavert-miniport-driver)、
  [MapAllocatedPages](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iportwavertstream-mapallocatedpages)。
- Microsoft: [EndpointBuilder](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-endpoint-builder-algorithm)、
  [physical connection](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-pcregisterphysicalconnection)。
- Microsoft: [OEMFormat](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/pkey-audioengine-oemformat)。
  INFの48 byte値は同資料の48 kHz / 16 bit / stereo例。
- Microsoft: [SYSVAD interface layout](https://github.com/microsoft/Windows-driver-samples/blob/main/audio/sysvad/TabletAudioSample/ComponentizedAudioSample.inx)。
  Wave/Topologyの別interface、Topology側EP設定を参照。Windows 10固有APIは導入しない。
- ボード一次ソース: 作業ルート port/gsi/kernel-4.14/sound/soc/mediatek/mt8167/
  の mt8167-afe-pcm.c（PCM hw_params、DL1 buffer base/end、pointer）、
  port/gsi/logs/ios-feasibility/windows-rt-audio-source-20261107/
  の mt8167-codec.c と mt8167-codec-utils.c（AU_LOLとcold-start DC）。
  新たな推測レジスタ書込みは追加しない。

## STOP/再開の一次ソース根拠

作業ルートの port/gsi/kernel-4.14/sound/soc/mediatek/mt8167/mt8167-afe-pcm.c:
- 774–816行、mt8167_afe_enable_irq: IRQ1のAFE_IRQ_MCU_CON bit0をenable。
- 818–851行、mt8167_afe_disable_irq: 同bitをdisableしAFE_IRQ_CLR bit0をack。
- 2218–2227行、mt8167_afe_dais_hw_params: retained bufferのBASEとENDを設定。
- 2358–2410行、mt8167_afe_dais_trigger:
  START/RESUMEはAFE_DAC_CON0のmemif enable、period counter、IRQ rate、IRQ enable。
  STOP/SUSPENDはmemif enableを0にし、IRQをdisable/ackする。
- 117–137行、mt8167_afe_pcm_pointer: 読取り成功のCUR==0はbuffer baseとして扱う。
  Windows側はMMIO読取り失敗をこのfallbackで隠さない。
- 66–78行のALSA hardware flagsにはPAUSE広告がない。
  Windows PAUSEは同じ停止とRESUME triggerに対応させる実装であり、
  本機でのcursor保持と再開波形は実機で別途確認が必要。

レジスタ対応は同Linuxのmt8167-afe-regs.hとDL1 memif定義:
DAC_CON0=0x10 bit1、BASE=0x40、CUR=0x44、END=0x48、
IRQ_MCU_CON=0x3a0 bit0、STATUS=0x3a4 bit0、CLR=0x3a8 bit0。
CURは読取り専用のままで、推測したresetレジスタは追加しない。
ソフトウェアのDPC世代とKeSynchronizeExecution/KeFlushQueuedDpcsはWindowsの寿命管理であり、
Linuxからバス排出保証を導いたものではない。

[WaveRT SetState](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-setstate)
のSTOP→ACQUIRE→PAUSE→RUNと逆順を扱う。RUN重複はidempotent。
実RUNの開始と停止はowner mutexでcheckout/checkin/PnPと直列化する。

## ローカル確認

既存fixtureを再使用し、実PCM/WAVEFORMATEXTENSIBLE、speaker topology、
STOP後の公開済みbuffer保持、parentのPFN/DMA一致と不一致拒否を確認。
再開では既存PnP fixtureをproduction transport/IRQ/DL1と接続し、
PAUSE/RUN、STOP/RUN、異なるportでの同じMDL再open、割当て/SG/voiceの一回保持、
旧世代DPC拒否とIRQ fault後のpoisonを確認。frontendも同じ既存fixtureを拡張した。
これらはハードウェアMMIO/DMA providerをfixtureに置き換えたホスト確認であり実機証拠ではない。
既存固定tone PnPとregistrationも確認。既存USB箇所のunused-but-set-variable警告は
host確認時だけ抑制し、USB実装は変更しない。
WDK compile/linkと実機動作の確認はこのbranch上では実施していない。

追加の確認: 既存allocation fixtureでkernelにWCを要求することと、
PARのNormal-NC/Device/Strongly-ordered/cached/fault/別pageの判定、
拒否時にゼロ埋めせずunmapすることを確認した。
既存EnginePcm fixtureの同MDL再open/STOP/RUNも通過。
hostのPAR値は明示的なfixture入力であり実機属性の観測ではない。
productionの照会helperはARM32 Thumb Windows COFFまでコード生成を確認したが、
DDI宣言を最小化した局所compileでありWDK compile/linkではない。
ログは作業ルートport/gsi/logs/ios-feasibility/windows-rt-audio-normal-nc-host-20261107.log。
一次資料PDF/TXTは同logs内windows-rt-audio-arm-ddi0406-20261107.*であり、repoへ混ぜない。

## IRQ fault後の停止要求（次候補、固定de214c8とは別）

ISRのMMIOエラーはengineをPOISONEDにする。従来のstopはRUNNINGのみを
受け付けたため、後続のSTOPからDL1 disable/IRQ mask/ackへ到達しなかった。
次候補では、実行済みDL1を所有するISRの同期範囲内で停止操作を一度要求する。
period通知counterのoverflowも同じ扱いにし、sticky fault後のISRでは再試行しない。
armedはfault DPCの通知まで維持し、PASSIVEのSetStateでMute/同期stopを試みる。
重複RUNを含め、その後の状態変更は失敗する。

stopはPOISONEDでもDL1 disable、IRQ1 mask、IRQ1 ackとreadbackを個別に試す。
先行操作が失敗しても後段操作を短絡しない。read-modify-writeのreadが失敗した
registerには推測値を書かない。以前のpoisonはreadbackが正常でも解除しない。
この関数の呼出元は当該DL1/IRQ1の排他的所有を維持する必要がある。
初回startの所有権precondition失敗を、この停止経路で強制回復することはしない。
DMA/SG/MDL/parent lease/D0は保持し、fabric drainや解放の成功は主張しない。

根拠はボードLinux `port/gsi/kernel-4.14/sound/soc/mediatek/mt8167/mt8167-afe-pcm.c`
の `mt8167_afe_dais_trigger` STOP/SUSPEND（2403–2410行）と
`mt8167_afe_disable_irq` IRQ1（838–842行）：AFE_DAC_CON0のDL1 bit1を落とし、
AFE_IRQ_MCU_CON bit0をmaskしてAFE_IRQ_CLR bit0をackする。
`mt8167_afe_irq_handler`（3541–3582行）もread失敗時にclear経路へ進む。
Windowsの実装では所有するIRQ1だけを操作し、他sourceはclearしない。

必要な既存DL1/IRQ/EnginePcm/PnP/first-playback fixtureを一括確認した。
WDK build、MMIO実動作、AudioSrv再生、可聴成功は未確認。
親ソース/INF/probeは変更していない。上記audio増分build recipeで
一致するEnginePcm親を再利用できる。既存de214c8/e4dff2b候補は置換せず、
採用する場合だけ新audio SYSのsource/hashを別manifestで記録する。

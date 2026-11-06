# MediaTek MT8167 AFE / MT6392 実PCM経路

## 到達点と制約

基点 a016d53 の first-playback は通常音声ではない。SILENT_DMA と
PCM_PATTERN が WaveRT 公開bufferとは別の common buffer に固定toneを生成する。
AudioSrv が書くPCMはそのDMAに届かず、WaveRT subdevice単独でspeaker topologyもなかった。

新しい EnginePcm は既定オフ。48 kHz / 16 bit / 2 channel の PCM と
WAVEFORMATEXTENSIBLE（valid bits 16、FL|FR）を受け入れる。
PortCls が割り当て、MmNonCached でmapした同一MDLをclientへ公開し、
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
音声INFは mt8167-afe-engine-pcm.inf（2026.11.7.1003）。
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

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

**通常音声の完成ではない。** 初回RUNだけを許可する。
PAUSE/STOPはmuteとDL1/IRQ停止を行うが、DMA drainを証明せず、
voice/parent D0/PortCls pages/SG mapping/backend referenceをresetまで保持する。
再RUN、再open後の再生、PnP再開始、suspend/resumeは未対応。
AudioSrvのprobeが初回RUNを消費した場合もresetが必要になる。
この制限を撤去するには停止後のfabric transaction、IRQ、codec状態を含めた
実際の再使用・解放契約が必要。

GetHWLatencyの既存ゼロ値は未校正のplaceholderである。
codec/fabricの実latency、AudioEndpointBuilderの実列挙、AudioSrv接続、
実PCM更新、可聴出力は未確認。
MT6392 hardware trimとLinux cold-start DC profileを引き継ぐが、
AU_LOL専用ADC実測校正を実装済みとは扱わない。

## 主担当向けWDK 8.1単位

この実PCM profileはparent verifierも変えるため、parentとaudioの組が必要。
既存のa016d53診断bundleを成功済みの実PCM版として置換しない。

    powershell -File tools/ci/build-mt8167-first-playback.ps1 -EnginePcm -IntegratedUsb -IntegratedWifi -SkipPeripheralBuild

単一parentのUSB/WiFi機能flagを維持してparent/audioだけをbuildする。
USB/WiFiのSYS、既存KS client、EDK2はbuildしない。
出力既定は build/mt8167-engine-pcm。別profileと同じ出力先を使わない。
署名・CAT・媒体配置・ACPIと実機操作は主担当が行う。
音声INFは mt8167-afe-engine-pcm.inf（2026.11.7.1002）。
既存mt8167-afe-first-playback.infは固定tone診断用のまま。

EnginePcmでは旧KS診断clientを自動起動しない。旧clientは自分でpinをRUN/STOPし、
固定tone判定を行うためAudioSrv経路の証拠にならず、初回RUNも消費する。
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

## ローカル確認

既存fixtureを再使用し、実PCM/WAVEFORMATEXTENSIBLE、speaker topology、
STOP後の公開済みbuffer保持、parentのPFN/DMA一致と不一致拒否を確認。
既存固定tone PnPとregistrationも確認。既存USB箇所のunused-but-set-variable警告は
host確認時だけ抑制し、USB実装は変更しない。
WDK compile/linkと実機動作の確認はこのbranch上では実施していない。

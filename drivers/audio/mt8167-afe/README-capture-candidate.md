# MT8167 内蔵入力 capture候補

run49の親1010/Audio1007/媒体/bootは変更しない。このbranchは別候補のmodule実装。
INF版・親版は予約していない。1012/1014その他の既存予約を流用せず、SYS/packageを
生成しない。録音endpointの実登録・内蔵micの実録音成功は未確認。

## 一次sourceと確定する経路

Benesse公開GPL `kernel-4.4/arch/arm64/boot/dts/mediatek/a04br3.dts` の
mt8167_audio_codecはMT8167 AFE/APMIXED/PWRAPとbus clockを使う。
sound machineはmt8167-mt6392だが、入力ADCは`mt8167-codec.c`のMT8167 codecである。
MT6392 codecを録音ADCと誤認しない。`mt8167_evb.c`のmachine routeは
Mic1→AU_VIN0、Mic2→AU_VIN2、headset→AU_VIN1。codec DAPMでは
AU_MICBIAS0→VIN0/2→左右PGA(CH0)→左右ADC→AIF TX、AFE/AD clockへ依存する。
`dmic-wire-mode=1`だけでは物理micがDMICであるとは証明できない。
DTS aud_pins_default/pmicclkは空であり、任意GPIOをmic-enableと仮定しない。
stock OTA 02.00.000 の `/vendor/etc/mixer_paths.xml` も初期値で
AIF TX=Analog MIC、AIN=INT ADC、O09 I03/O10 I04=1、builtin-micでは
左右PGA=CH0を選ぶ（SHA256
`75779a31ffca5563a42c0135ae4a126f1fdfa445b8a74d5dcc7c30c62b82de07`）。
これは実機向け設定の経路根拠であり、micの物理実装数・録音成功の実測ではない。

`mt8167-afe-pcm.c` memif_data[VUL]ではBASE80/END88/CUR8c、DAC_CON1のfs16/
mono27、DAC_CON0 enable3、format PBUF22、IRQ2 fs8/count3b0/clear1。
INT ADDAのI03/I04→O09/O10→VUL、S16 stereo/48kに限定してul_engineへ実装した。
ADC powerdown/analog codec設定をこのmemif engineの成功で代替しない。

## 実装済みmodule

- ul_engine: 実VUL ringのBASE/END、48k形式/ADDA/接続、IRQ2、実CUR位置、停止/再開。
  exclusive AFE admissionを要求し、DL1/HFP/他memifとの同時使用を広告しない。
- wave_dma_live: device→memoryはGet/PutScatterGatherList両方FALSE、
  KeFlushIoBuffers ReadOperation=TRUE。旧renderのTRUE契約は保持する。
- capture_owner: PnP既存AFE mapping/実DMA adapterだけを使う。PortCls連続pagesを
  検証済みNormal-NC/WCでmap、実RX DMA、IRQ同期、STOP→RUN同一ring再利用、
  position rundown、stop後もmapping/leaseを保持。bus drainを捏造しない。
- WaveRT: capture OUT streaming pin、ADC node、analog bridge IN。
  topologyはmicrophone IN→audio OUT、physical connectionはtopology1→wave1。
  登録nameは全てstatic lifetime。既存render記述と成功広告は変更しない。
- GetHWLatencyはcapture専用providerが実値を返したものだけ。未知FIFO/codec delayを
  0として登録しない。古いrenderの診断契約とは分ける。

親側portable module `codec_capture.c`は、実APMIXED UL clock/Vcm/Vref/micbias0/
CH0 PGA/ADC、AFE UL rate latch/AD_CLK/TXをreadback付きで実装する。
codec fs=1、ADDA fs=3、VUL/IRQ fs=10を混同しない。
既存VAUD rails/audio clock ownerを前提にし、PMIC/GPIOを勝手に追加操作しない。
このmoduleを親callbackへ結ぶには独立新版の予約とmatched-parent flags保持が必要。

## 未接続の親契約（既存親へ偽装接続しない）

Mt8167CaptureResourcesのacquireInputは既存AcquireMutedとは別物である。
親所有のaudio clocks/rails、APMIXED ADC/PGA/micbias、codec UL sample-rate toggle、
board analog route、AFE mask/IRQ2排他を実装し読戻し検証する必要がある。
queryLatencyも実FIFO/codec timingが必要。現在のv11 playback interfaceにこれらは無い。
実providerを取得しないままCaptureCreate/PortCls登録を行うPnP glueは追加していない。
失敗を成功扱いするproviderは作っていない。レジスタengineの成功はmic存在の証明ではない。

Nash HFPは23ee ABI/provider v2 checkpointを保持。HFP DAI/IRQ2とVULの同時所有は
禁止。物理radio一致/FC72 PCM/独立DMA drain/codec latencyの未資格は保持する。
4 base audio portsとHFP4 portsを無条件に同時登録しない。将来統合には枠と新版予約が必要。

## 親側入力ownerの接続候補

`capture_input_owner` は既存親のresource lock内/PASSIVEで、排他予約→既存
VAUD/audio clock取得→実codec UL準備を行う。既存playbackのAcquireMutedを
録音準備の代用にしない。予約はrender/HFP/他AFE ownerも排他にする。
変更途中の失敗では電源・予約を保持しpoisonを残す。online停止は新取得を禁止し、
既存leaseの独立DMA/IRQ drain検証後の解放は可能にする。
codec停止時は取得前のCH0選択field・UL rate・AD_CLKを復元し、rateは現在の
statusから再latchしてからcodec clockを戻す。STOPレジスタだけでdrain成功にはしない。

このstate machineは親WDF callbackへ未接続。実resource予約/電源/drain callbackと
実FIFO/codec latencyの根拠が揃うまでcapture endpointの生成は行わない。
親版未予約、既存1010/Audio1007およびtrial51のbytesは変更しない。

## 親1020接続作業の中断checkpoint

親1020予約（main指示）。1010/SD1をmergeした基点7dccbd87へ、独立capture
QUERY_INTERFACE、親clock/VAUD/ADC callback、lazy取得、PortCls録音pairのPnP接続、
固定KS録音client草稿を追加。GPU媒体優先への担当切替で中断した。
このcheckpointはcompile未実施・build recipe未接続で、署名候補ではない。
親/playbackとcaptureは最初のstreamが排他取得してresetまで保持する。
GetHWLatencyはvoidで未対応statusを返せないためKS queryの未対応gate草稿を含む。
WaveRT一般互換/latency仕様適合は未検証。未測定ゼロlatencyは公開しない。
51mediaと配置済み親1010/Audio1007は不変。

# MT8167 AFE / MT6392 speaker1034：vendor nominalと実測精度の分離

run143のInitialize80070032は実RT8.1 AudioKSEがbuffer後に必須HWLATENCYを要求するのに、adapterが全経路共通で拒否する契約不備。前段の「実測校正なしでは標準値を返せない」はMicrosoft契約から導けず訂正する。CodecDelayはrate依存のbest estimateで、校正/実機実測の必須APIではない。ULONG三値にはunknown sentinel/valid maskがなく、MAXULONGは未知表現として返さない。0は公式unknownを意味しない。

## 本機speakerの報告モデル

CT3 GPL7c478212 `mt8167_afe_hardware` は `.fifo_size=0` を明示、rate/period/ring容量と別。`soc_pcm_pointer` は追加delay/codec_delayを0初期化し、CPU/codec/platformのdelay callbackがある場合だけ加算する。対象mt8167_afe_pcm_ops、AFE DAI ops、mt8167_codec_aif_dai_opsはdelay callbackを持たない。MT8167内蔵DAC→AU_LOL→MT6392 Int Spk Ampはstock machine routeで、MT6392はアナログampでありI2S外付けcodecではない。現Windows speakerも同じ48k stereo S16 DL1/nativecodec経路を使う。

この実vendor報告モデルをFifoSize0/ChipsetDelay0/CodecDelay0へ移す。3値とも実測ゼロではなく、vendorが追加FIFO/CPU-memory packet-service/codec補正を報告していないnominal値。MicrosoftのlegacyPCI0の規則をMT8167へ流用しない。silicon内部のpipelineが物理的に0という主張でもなく、HLK低遅延/同期精度資格でもない。ring12288BやIRQ period・AFIFO48KiB・depop/trim待機時間をFifoSize/CodecDelayへ代入しない。

stockのPBUF32/64/128/256は共通SoC実装に存在するが、本機専用AFE driverはPBUF bit16..17をsample formatへ使い、FIFOサイズ設定は行わない。異なるSoC実装のbit0..1を盲目にwrite/実読解釈せず、今回nominalFIFOは実採用driverの宣言0を採る。8x upsampleとnative48kのrate設定は分かるがFIR tap/order/group-delayは未提供であり、任意frame数を足さない。

## 実装範囲

標準KSはPortClsへ戻し、adapter-wide HWLATENCY gateを呼ばない。speakerだけ専用miniport factoryでnominalを選び、GetHWLatencyはこのvendorモデルを返す。DMA/IRQ/実CUR/STOP/retain/共有親owner/restoreを不変更。consumerは元の共有WASAPIとエラー判定を保持、Initialize成功合成やexclusive/directKSへの逃げ道なし。

HP/captureはspeaker factoryを使わない。未知MAXULONGのlatencyをNewStreamで拒否し、speakerのnominalで流用しない。実sense、A/B/C校正、parent/QI、単一ownerは保持。未接続HP/未測定HFPへ本修正を拡張しない。6Nameの常駐とMEDIA GUID10318を保持。新INF1034/NativeStart1034、親変更なし。

## loopbackと限界

stock `mt8167_codec_aif_dl_ul_lpbk_event` はABB_AFE_CON2 bit3でAIF RX→TXへ折返し、DAC/MT6392 speaker出力を通らない。AMIC/DMIC→SPK/HPはmic入力を再生へ流す経路であり、speaker terminal→ADCの内部帰還ではない。MRGも音声digital transportでanalog出力遅延を測らない。既stockに本機speaker出力まで含む自動loopback測定routeは確認できず、これをoutput実測producerとして追加しない。物理ケーブル/AP/ユーザー操作要求なし。

[Microsoft KSRTAUDIO_HWLATENCY](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-ksrtaudio_hwlatency)、[GetHWLatency](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/portcls/nf-portcls-iminiportwavertstream-gethwlatency)、[Developing a WaveRT Miniport](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/developing-a-wavert-miniport-driver)。mainの保存本文windows-rt-hwlatency-primary-main/0.txtとrootsource-evidence.jsonに対応。これは標準streamを可能にするnominal候補であり、実機再生/音出し/latency精度はmain通常batchで未確認。

# Samsung AJTD4R / MT8167 MSDC0 1.10要求内overhead候補

基点はfresh144の実SYS b3ce17a8a36b1afba25ca53503735f57aba4b47723357a960169dd47b36f9bdaと同byteの署名1.9/build source c6b5e319。branch55d03220はその後のtest-only変更でruntime9fileは同一。現145storage、親USB/GPU、mediaを変更しない。service/HWID/BOOT/groupとAJTD4R命名を保持し、新INF0.0.1.10だけ独立候補として署名する。

## 変更

A04_MSDC_REQUEST_QPC_REUSE/REQUEST_COPY_OPTはいずれも既定0、候補buildのみ1。既存REQUEST_MDL_REUSE=1を保持。

Microsoft KeQueryPerformanceCounter一次契約はfrequencyがsystem boot時固定かつ全CPU共通でcache可能と明記する。要求内のLARGE_INTEGERだけへ最初の有効frequencyを保存し、4KiB要求8sectorの取得を8回から1回へ削減する。running counter、各sector/各poll段階のstart・10秒期限・iteration上限・失敗診断は従来どおり。MMCのclock/PLL/voltage/timingを取得・変更する処理ではない。初期化/GPT等の単sector wrapperは従来と同じfrequency取得を行う。

要求data↔512B bounceの逐次byte loopを、非重複時だけRtlCopyMemoryへ接続する。Windows一次契約の非重複/IRQL条件に沿い、既存mapped SRBとresident bounceを同一worker内で使う。重複/同一bufferは従来forward byte copyへfallbackして意味を変えない。各sectorのKeFlushIoBuffers、DMA/CMD17/CMD24/CMD13/STOP、copy方向・失敗時copy抑止・cancel/GPT guard・senseを保持。

## source照合と見送り

LinuxCT3 remote https://github.com/s1204IT/CT3_GPL_LGPL_Src source7c478212 sd.cはSG/multiblock/DMA/IRQ/clock制御を実装。XNU remote https://github.com/mouseos/NativeMsdc.git sourcef2fc17f6はpersistent buffer/bulk memcpy、owned IRQ wait/fallback/batchingを持つ。これをWindowsのDMA owner/stop/power契約に直接置換しない。

現1.9は4KiB SRBを512B CMD17/CMD24へ分割、queue1/pollingであり大きな効率制約は残る。MDL要求内再利用は既に実装済み。multiblock/SG/IRQ化は既存候補があるが、STOP失敗時MDL/adapter/cache ownership・rundown・power復帰未確定があるためgateを外さない。write retry、unsafe cache flush、MMC clock/電圧/tuning推測変更は行わない。

## 検証境界

ASan/UBSanで実wrapper/RunIoのbaseline19、flagsOFF、flagsONを比較する。165件のREAD/WRITE1..8block、各sector失敗/cancel、MDL OOM、bad frequency、範囲拒否等についてdata/status/sense/MMIO/DMA/cache-flush列・診断・MDL収支を比較する。別6copy配置で非重複/重複fallbackを確認する。4KiBのMDL1回は維持、frequency取得8→1をassertする。hardware DMAとOS APIはmock。改善MiB/sやCPU使用率・実boot/実I/O成功は未測定。

新runnerの既存WDK小closureでminiportと実1.9 DMA coreの2TUだけcompile、portable5coreはsource/include/object hash一致で再利用する。storport import libraryは実1.9 SYSの6importsと実RT8.1 exportsに限り、型/呼出し宣言はWDK8.1を使う。既署名1.9 SYS/INF/CATのtrust/membershipも再確認する。SDK/WDK/PCO全setupなし。結果/再現入力/未達は成果pins/HANDOFFへ固定する。worker実機非接触、145storage保持。

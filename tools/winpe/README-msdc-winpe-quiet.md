# 準備WinPE自身のeMMC trace抑制package

run89のWindows更新用quiet1.8を変更するものではない。準備WinPEが読み込む旧SYSは別物で、既存媒体の`/a04br3/mt8167-msdc.sys`はSHA256 `28115a3ba588d3354e30faca4b37a7de28dcc0b288c7354e2dd3b1ddf3ef416f`。INF0.0.1.6、source `2371bd09aa6f90b60f4affeaef83553451713eb1`はWDK8.1増分再ビルドで全PE section byte一致を確認した。通常Windowsの元SYS `fb4d168b...`と混同しない。

## 確定した変更

既存署名quiet1.8/source `e2db926f1ddd12b4cae341a7df50f832e20c16e5`のSYSを**byte保持**で再利用する。低水準DMA修正や既定OFF化を重複実装しない。既存実装のnormal trace=0 / DMA poll trace=0を準備側へ適用するためのpackage構成修正である。

`build-msdc-winpe-quiet.ps1`は入力signed-manifestの明示hash、全INF/SYS/CAT hash、既存証明書、SYS/INF CAT所属と固定SYS hashを検査する。旧WinPEのdrvload pathを保つためINFを`a04-msdc-probe.inf`名で格納し、その**名前に対する新CAT**を生成・署名する。元CATのままINFをrenameしない。SYSは再compile・再署名しない。

同一packageに対応する`observe-msdc-storage.exe`をWDK8.1 ARMで増分compile・署名する。既存read-only observer/source `c10ace9f26b0a9e389dd2dcc6ba77cc92b9b68c3`の実装を使用し、3つの固定hashだけを生成headerから受け取る。pinを緩めたり任意pathへ変更しない。旧observerは旧SYS/CATに固定されているため同時置換が必要。

```powershell
powershell -NoProfile -File tools\ci\build-msdc-winpe-quiet.ps1 `
 -StoragePackage <fixed-quiet1.8-package> -StorageManifestSha256 <explicit-sha256> `
 -OutputDirectory <checkout内新規directory> `
 -CertificateThumbprint BCBF6C456C6F0DD0818434763B189488FBF13570
```

今後の新規準備媒体では、packageのINF/SYS/CAT/observer/certを一組で`/a04br3`へstageする。WinPE証明書storeへ既存公開証明書を標準手順で登録し、WIM内ファイルhashとobserver header pinの一致を検査する。旧run89媒体、通常WindowsのSYSTEM/installedSYS、boot/GPTは変更しない。これはpackage候補であり、新WIM生成・実機投入・WinPEの選択確認は別工程。

## DMA待機で確認できた範囲

- Linux CT3 source `7c4782127d4ea11965be00ce1343056be9bfb769`の`kernel-4.4/drivers/mmc/host/mediatek/mt8167/msdc_reg.h`はXFER_COMPL=bit12、DXFER_DONE=bit13、DMAQ_EMPTY=bit6。`sd.c`のdata完了もXFER_COMPLを使用する。XNU `NativeMsdc.h`のkIntXferDoneもbit12。
- INT=0x2040はDXFER_DONE|DMAQ_EMPTYで、bit12の証明にはならない。DMA_CFG=4はSTS bit0=0だが、転送成功とは別。0x3040を含む後続pollも保存UARTにある。既存WRITE-PERFORMANCE.mdの記録と整合する。
- `A04DmaSector`は**512byte sectorごと**にpoll report数を初期化する。複数sectorの1つのSRBでも多量になる。pollsはhexの0x1000/0x2000/0x3000（4096/8192/12288回）。elapsed-lowはKeQueryPerformanceCounter ticksの下位32bitで、microsecondsではない。
- タイムアウトはfrequency*10 ticksと100000000 iterationの二重上限。開始時刻はFIFO/command/data/STOP/DAT0/command busy/CMD13の各段階で更新され、request全体の固定10秒sleepではない。毎request timeoutしたという証拠はない。
- `A04_DMA_DISK_PROBE`経路でDMA失敗するとtransfer failure/senseを返す。PIOはcompile時の別経路で、DMA失敗後の自動fallbackではない。
- DMA error/init/checkpointを残す。今回のpoll抑制は新しい準備packageだけに限定する。完了bit、W1C、timeout、cache/MDL、guard範囲、worker、キュー、リトライ、DMA停止処理は変更しない。

UARTの混線・出力量は実観測。出力を除去することによる速度改善量は未測定で、DMAそのものの待機原因を解消したとも扱わない。DMA成功/失敗の未観測値を補完しない。

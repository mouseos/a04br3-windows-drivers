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

## private base生成

`build-msdc-winpe-quiet-base.py`はoriginal baseのWIM/ESP SHA256、91ee3ce6署名package manifest、WDK8.1の署名/CAT所属receiptとその生成recipe hashを固定検査する。新しい出力directory以外へ書かない。alias INF/SYS/CAT/observer/公開certのflat5fileと、Root/TrustedPublisherへ同じ公開certを追加したWinPE SOFTWAREだけを置換する。WinPE SYSTEMは変更しない。旧署名者も含め既存store全entry、その他全WIM record、BCD/EFIを含むWIM以外の全ESP fileを前後比較する。

regipyの値型はWin32の数値3ではなく文字列`REG_BINARY`を返すため、その型名とBlob byteを完全一致で検査する。型・member/hash gateを省略しない。未コミットbuilderでは生成を拒否する。既存`replace-esp-wim.py`（source074と同byte）を再利用し、WIM verifyとFAT修復後の読取り検証を行う。

membership receiptは`tools/ci/verify-msdc-winpe-quiet.ps1`を既存WDK環境で一度実行して回収する。固定packageのSYS/observer/CAT署名・署名者、alias INF/SYSのCAT所属を全て成功した後だけreceiptを生成する。Linuxのosslsigncodeではalias INFのCAT所属検証に未対応のため、それを無検証で通過させず、固定package hashに結合したWDK receiptを必須にする。

出力は`maintenance-staged.wim`、`maintenance-staged-esp.img`、`manifest.json`。これは次回更新の**新base入力候補**であり、旧base hash固定のGPU/private media builderへ無条件に投入できるものではない。次のbuilder統合では、この新base manifestと生成出自を明示して専用型として追加する必要がある。original base許容gateの削除や任意base許可は行っていない。起動・速度改善は未測定。

## 最新Engineとの統合と生成済みbaseの引渡し

ObserveOnly source66996e88を起点とする独立branchで、この生成経路と
`preparation_base_input.py`の固定selectorを統合した。run93 source/媒体は編集しない。
新builderの`--preparation-base-manifest`、`--preparation-base-manifest-sha256`、
`--preparation-base-membership-receipt`をすべて明示したprivate媒体だけでquiet baseを選べる。
指定無しは従来originalのhashに限定し、任意baseを受け入れない。

生成済みbaseの正本はmanifest be3bdabe、WIM dee7bf64、ESP0e23cdb1。
`verify-msdc-winpe-quiet-base.py`はこれらの実体をoriginalと再比較し、flat5file、
SOFTWARE公開証明書追加、既存store全値、変更外WIM stream/security/attrs、
WIM以外の全ESP file、BCD、WinPE SYSTEM保持を確認する。
旧準備source2371bd09と再利用quiet e2db926fのDMA/PIO/CDB/SCSI/guardソースのbyte一致も確認する。
既存SYSは再compile・再署名せず、member/signature正本receiptへ結合する。

これは完成済みの**準備WinPE base WIM/ESP**と次更新builder入力の引渡し。
run93開始後にfresh92を使って別のOS SYSTEMや実行可能transactionは生成しない。
次のnative SYSTEM・target/client結合はmainが返却するrun93後の新freshを使う。
DMA成功や速度改善量、実機での選択driverは未検証。

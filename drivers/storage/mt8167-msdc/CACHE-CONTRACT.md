# MT8167 MSDC0: cache-disabledとflushの根拠

調査・修正基点: drivers repo `d6dbb37`。専用branch
`work/mt8167-msdc-cache-contract-20261107`。BTは別branchの`bf98963`で保全済み。
実機/runner/REA操作、GPU/Audio/共有親/UEFI sourceの変更は行っていない。

## 結論とrun33の扱い

**書込み経路は元から実EXT_CSD[33] bit0=0を必須としていた。**
`A04Initialize` がPIO CMD8で512byteのEXT_CSDを取得し、cache_disabledを設定。
`A04ValidateWindowsRange` はその条件がなければ書込みを許可しない。
このsourceと一致するbinaryでwrite_readyが成立した初期化では、cache enabledの
まま書いていたという仮説は成立しない。SCSI MODE SENSEの固定WCE=0だけからの
推測ではなく、実媒体読取りを根拠とする。

ただし旧sourceのrestartはEXT_CSDを読み直してもCACHE_CTRLを照合せず、失敗状態を
解除していた。またSYNCHRONIZE CACHEはpure CDB decoderが直接successにし、
SRB FLUSH/SHUTDOWNは多くの構成で未処理だった。これを修正する。
**CACHE_CTRLへ書くCMD6、FLUSH_CACHEへ書くCMD6は追加しない。**
cache=0を維持するこのdriverでは、Linuxもdevice cache flushを不要とする。
cache=1/未知の媒体を無根拠に無効化・flushして受け入れる拡張はしない。

run33についてのユーザー観測: FlushFileBuffers成功とservice 21終了をUART確認後、
Recoveryへreset。logs directory内の全fileが消失し、attempted/pending/diagnostic
markersとrootbrightness.logは存続。この偏りだけでeMMC障害やcache lossとは断定しない。
ファイル/ディレクトリmetadata、cleanup/servicing、volume選択等の可能性も未確定。
本source調査ではrun33のinstalled SYS hash/正確なbuild flagsと、生のCACHE_CTRL数値を
新たに実測していない。上記のsource条件を無条件にrun33の実機値0と読み替えない。

## Linux MMC一次source

`CT3_GPL_LGPL_Src` commit `7c4782127d4ea11965be00ce1343056be9bfb769`。
以下はその`kernel-4.4/`からの相対path。

- `include/linux/mmc/mmc.h:285`: FLUSH_CACHE=32、CACHE_CTRL=33、CACHE_SIZE=249。
- `drivers/mmc/core/core.c:3448`: `mmc_flush_cache`はMMC、cache_size>0、
  cache_ctrl bit0=1の場合だけCMD6のFLUSH_CACHE=1を実行。disabledなら何もしない。
- `drivers/mmc/core/mmc.c:1695`: cache_size>0ならLinuxは明示CMD6でenableし、
  成功時だけcache_ctrl=1に更新する。cache搭載とcache有効は別。
- 同`:1757`: `mmc_cache_ctrl`。enableはgeneric_cmd6_time、disableはflushを伴うため
  timeout=0を下層へ渡す。根拠なしの短い固定CMD6 timeoutや書込み再試行は追加しない。
- `drivers/mmc/core/mmc_ops.c:482`: `__mmc_switch`はR1b/busy、CMD13 statusと
  switch errorを扱う。CMD6受付だけを永続化成功にしない。

## 現Windows UEFI sourceとの照合

参照checkout: `port/edk2/edk2-mtk-a04br3-windows-rt`、参照時HEAD
`e8af0fb0a2cdf2b592ad16d63b4ad947a993a94b`。実機FDの出自と同一と断定しない。
`Platform/Benesse/A04BR3Pkg/Drivers/EmmcBlockIoDxe/EmmcBlockIoDxe.c`は
`A04BR3_EMMC_BUILD`で隣の`SdCardBlockIoDxe/SdCardBlockIoDxe.c`をincludeする。

同sourceの`SdInitializeCard:1514`はCMD0/CMD1、RCA1、CMD7、実EXT_CSD読取りを行う。
`EmmcReadExtCsd:1320`、`EmmcSwitchByte`とhigh-speed設定ではbus width/HS timingを
設定するが、CACHE_CTRL/FLUSH_CACHEの書込みはない。`SdWriteBlocks:1888`は
EFI_WRITE_PROTECTED、`SdFlushBlocks:1901`はEFI_SUCCESS、mediaはReadOnly/WriteCaching=false。
**これはUEFIがwriteしない契約であり、Windowsの永続化の証明としては使用しない。**
Windows miniport自身のlive EXT_CSD証拠が必要。

## 修正後の実行経路

- `cache_policy.h`にfull EXT_CSD読取りから得るraw CACHE_CTRL、CACHE_SIZE、revision、
  user partition、capacityを保持。CACHE_CTRLは予約bitも含め厳密0を要求する。
- initialize時に古い証拠を破棄。既存の成功したEXT_CSD読取りからだけ更新し、
  UARTへ数値 `CACHE_CTRL/CACHE_KIB/EXT_REV/USER_PART/DISABLED_PROVEN` を出力する。
  CID/serial/個体dataは出さない。
- `scsi.c`はSYNCHRONIZE CACHE(10/16)を`A04_SCSI_FLUSH` actionへ渡す。
  miniportの単一I/O gate内で、failedでなく、現媒体のcapacity/partition/cache証拠が
  一致した場合だけ完了。IMMEDは従来通り拒否。MODE SENSEのWCE=0も同じ証拠が必要。
- SRB_FUNCTION_FLUSH/SHUTDOWNも同じgate/workerを通し、未確認ならSRB error。
  CDB flushはNOT READY senseを返す。decoder受付やSRB受付だけで成功としない。
- CMD24は既存`dma.c:154`以降のDAT0 high、CMD13 READY_FOR_DATA/TRANを確認後に
  完了する。先行するaccepted I/Oをgateで直列化し、cache=0なら追加のCMDは不要。
  既存poll上限/エラー伝播を維持し、flushに新しいmedia wait/再試行を追加しない。
- transfer failure、stop、resetで証拠とwrite_readyを破棄。restartはfailedのまま
  既存GPT/EXT_CSDを再読し、容量とcache=0を照合後に既存GPT guardで書込み範囲を
  再検証してから再許可する。enabled/不明ならrestartを失敗させる。
- 中断・failed状態のflushはsuccessにしない。デバイスの物理寿命やStorport worker/
  DMA mappingの既存制限をこの修正で解決したとは扱わない。

新規media書込みcommand、cache enable/disable、flush cycleを追加しないため、
cache検証修正によるeMMC書込み寿命への追加負担はない。restartのGPT再検証はreadのみ。

## 検証と主担当への引渡し

既存`scsi_test`にflush action・proofの有効/失効/非user/容量不一致の確認を集約。
既存`dma_test`で書込み完了待機のregressionを確認。追加の模擬test群は作らない。
ARM32 public reference headerでdefault/read-only/boot-write/boot-passive/sync-PIOの
変更TUをcompileする。実WDK8.1最終link・署名・実機永続化検証とは区別する。
既存`tools/ci/build-wdk81-probe.ps1`が同じsourceを使用し、追加TU/dependencyは不要。
helperのRUNNER_TEMP/TEMP/TMPは主担当のcheckout内へ設定してからビルドする。

主担当はinstalled storage SYSの出自を合わせ、通常の署名ビルド/配置が必要。
本担当は実機/runnerへ接続しない。次のUARTでは新しいraw数値で0を確認できれば、
その起動についてcache-enabled仮説を否定する。run33のlogs消失の原因は未解決のまま。

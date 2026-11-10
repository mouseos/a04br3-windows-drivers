# LC709203F battery gauge

TAB-A04-BR3はI2C2のアドレス0x0bにLC709203Fを接続し、同じバスを
BQ24160充電器と共有する。`gauge.c`は読取りのPEC検査、動作モード・電圧の
妥当性検査、および端末Linux実装に記された残量の100〜950スケーリングを
提供する。PEC方式はMITライセンスの[UniXNU A04BR3GaugeCore.h](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3GaugeCore.h)を参照。
バッテリープロファイル、充電器、アラームへの書込みは行わない。

`wdf_gauge.c`は専用I2C2リソースを持つ既定OFFのWindowsバッテリー
ドライバ試験版。現時点では既に設定されているコントローラしか扱わず、
I2C2を充電器や他のドライバと同時利用できない。UEFI `5ef1ff4` と
ドライバ `9a95c80` の3コアWindows RT WinPE実機試験では、PEC付き取得、
バッテリークラスの開始、およびWindowsが返した残量39%を確認した。
搭載Windows、4コア、スリープ復帰と長期安定は未検証。輸送・PEC失敗時は
サンプルを無効化する。
PEC・値域・動作モードが不正でもI2Cトランスポートが正常なら、30秒後の
次回読取りで回復を試み、有効な値が得られるまで古い残量を返さない。
コントローラ使用中・NACK・タイムアウト等のバス障害は従来どおり固定し、
コントローラの復旧を推測して再使用しない。初回の不正サンプルからの回復、
各障害の実機動作とPnP資源変換の変更は未検証。PnPのraw資源でI2C2を識別し、
OSが変換した排他メモリ資源のみをマッピングする。
`mt8167_i2c.c`にrepeated-start読取りの基礎を用意しているが、コントローラの
初期化・ロック・障害復旧は呼出し側の責任であり、実機では未検証。
`battery_class.c`はMicrosoftのバッテリークラス用コールバックで、
未取得の充放電状態・電流は推測しない。試験中のWindows電源状態は実際の
USB接続中にもACオフラインを報告したため、充電・給電判定には使用できない。
PnP登録、サンプルの更新・失効と通知は試験版へ接続済み。電源遷移、
適切な充電器連携と長期安定は未検証。

BQ24160との共有所有者の試作は `../bq24160/README.md` を参照。
`A04_BQ24160_EXPERIMENTAL=1` ビルドは引継ぎの安全ゲートにより
D0開始を拒否する。既定の燃料計単独構成はBQへアクセスしない。

## 初回起動失敗checkpoint（1.7 source64cからの独立変更）

この変更はCode10の原因修正ではなく、次の一括試験用の観測である。
型番・INF・driver version・I2C転送/reset/retry/成功判定は変更しない。
ビルド・署名・実機確認は別途必要。現時点では未実施。

DriverEntryで1回だけload epoch、最初の負NTSTATUSで1回だけ失敗recordを
サービスの `Parameters\NativeCheckpoint` にREG_BINARYで保存する。
サービスpathはDriverEntryのRegistryPathを使う。通常のサービス名は
`LC709203F`。正常pollや成功callbackでは書かない。
記録書込み失敗は既DDIの戻り値を変えず、DbgPrintExで失敗だけ通知する。
registry flushは行わないため急停止での永続化保証はない。
最初の失敗claim後は書込み失敗時も再試行しない。

- `LoadEpochV1`: 16B、LE ULONG4個 = magic `0x3146474c`、version1、
  load FILETIME low/high。ロードを跨いだ古い失敗値を消す代わりに区別する。
- `FirstFailureV1`: 96B、LE ULONG24個。index0..2=magic/version/96、
  3=callback、4=NTSTATUS、5..6=load FILETIME、7..8=failure FILETIME、
  9=gauge faulted、10=adapter faulted、11..14=fault reason/command/status/fifo、
  15=admission valid bitmask、16..20=START24/TIMING20/DMA48/IO_CONFIG40/HS60、
  21=previous WDF power state（D0Entry時）、22..23=reserved0。
- callback: 1 DriverEntry、2 DeviceAdd、3 PrepareHardware、4 D0Entry、
  5 SelfManagedIoInit（BatteryClassInitializeDevice）。ReleaseHardware/D0Exitは
  現sourceでは常に成功のため対象外。通常IOCTLの拒否は起動失敗として記録しない。
- fault値は既contextをbus lock下でコピーする。admission値は既判定のread結果を
  保存するだけで、short-circuitで未読のbitは0。追加MMIO/転送/待機はない。
  callbackと独立した既fault値を、そのcallbackの原因と自動的に断定しない。

### 既collectorと同一loadで必要な最少読取り

既fresh collectorのSYSTEM/SYSTEM.LOG1/SYSTEM.LOG2回収で原値は保存される。
追加の端末collectorや別hiveloadは不要。次のsingle-load native inventory側で
`ControlSet001\Services\LC709203F\Parameters\NativeCheckpoint` の固定2値だけを
read-onlyで読む（現readerのOpenPath whitelistにはこのexact pathの追加が必要）。
kind=Binary、長さ16/96Bを厳密に検証してraw hexを保持する。全Parameters再帰は不要。
既selected INF/version/SYS hash、Select.Current、boot/driver epochと同じreceiptへ結合する。
本branchの古いinventory入口を現在のGPU試験へ置換しない。

`tools/recovery/decode-lc709203f-first-failure.py --epoch-hex <16Bhex>
--failure-hex <96Bhex>` で保存値をdecodeできる。failure値欠落時は引数を省略する。
load epoch不一致はstale、欠落はunknownであり成功ではない。
FILETIMEは時計補正され得るため、数値の前後関係だけで因果・bootを断定しない。
SYS/hashとload epochを結合し、最初の失敗callbackとstatusから次の修正を決める。

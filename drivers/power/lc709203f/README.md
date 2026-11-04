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
`mt8167_i2c.c`にrepeated-start読取りの基礎を用意しているが、コントローラの
初期化・ロック・障害復旧は呼出し側の責任であり、実機では未検証。
`battery_class.c`はMicrosoftのバッテリークラス用コールバックで、
未取得の充放電状態・電流は推測しない。試験中のWindows電源状態は実際の
USB接続中にもACオフラインを報告したため、充電・給電判定には使用できない。
PnP登録、サンプルの更新・失効と通知は試験版へ接続済み。電源遷移、
適切な充電器連携と長期安定は未検証。

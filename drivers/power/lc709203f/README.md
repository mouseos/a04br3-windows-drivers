# LC709203F battery gauge

TAB-A04-BR3はI2C2のアドレス0x0bにLC709203Fを接続し、同じバスを
BQ24160充電器と共有する。`gauge.c`は読取りのPEC検査、動作モード・電圧の
妥当性検査、および端末Linux実装に記された残量の100〜950スケーリングを
提供する。PEC方式はMITライセンスの[UniXNU A04BR3GaugeCore.h](https://github.com/mouseos/UniXNU-a04br3/blob/main/package/A04BR3GaugeCore.h)を参照。
バッテリープロファイル、充電器、アラームへの書込みは行わない。

これはWindowsバッテリードライバではない。I2C2のPnPリソース取得と共有
排他、電源遷移、Windowsバッテリー
クラスへの登録と通知の実装が必要。mock以外では未検証であり、残量表示が
実機で動いたことを示さない。輸送・PEC失敗時は古い出力を更新しない。
`mt8167_i2c.c`にrepeated-start読取りの基礎を用意しているが、コントローラの
初期化・ロック・障害復旧は呼出し側の責任であり、実機では未検証。

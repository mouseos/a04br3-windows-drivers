# 次回eMMC更新の命名準備

主担当がRecovery読取りで保存したname=AJTD4R、manfid=0x15、oemid=0x0100を使用する。
既存Linux CT3 kernel4.4 drivers/mmc/card/block.cのCID_MANFID_SAMSUNG=0x15とも照合する。
マーケティング型番・容量・製品系列へ推測で展開しない。

既存署名quiet1.8 INFのProviderはTAB-A04-BR3 developmentである。汎用source INFには
既にmouseosがあるが、署名済み実packageの値と混同しない。
`prepare-msdc-ajtd4r-metadata.py`は固定署名manifest/全files/SYSと実採取logのhashを確認し、
Provider=mouseos、DeviceDescription/ServiceDescription=MediaTek MT8167 MSDC0 / Samsung AJTD4R eMMCを用意する。
変更はStrings4項目だけ。逆置換して元INF全byteに一致することを確認する。

成果物は**mt8167-msdc.inf.inという未署名テンプレート**。版は元1.8のままなのでinstall入力ではない。
次の実eMMC更新時に明示した上位DriverVer/dateで新CATを作成・署名し、INF/SYS所属を再検証する。
署名済みSYSは再build・再署名せずbyte保持する。既存CATは変更後INFへ流用しない。
準備WinPEのa04-msdc-probe.inf aliasを変更する場合も、alias名でCATとobserver pinを一組で作り直す。

run95 GPU媒体、quiet WinPE完成base、現eMMC1.8、署名packageは変更しない。
実機操作・新媒体生成・次回eMMC更新の実行はこの命名準備に含まれない。

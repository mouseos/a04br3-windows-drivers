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

次回実更新用の再包装recipeは`tools/ci/package-msdc-ajtd4r-metadata.ps1`。
固定quiet1.8署名packageと準備済みmetadata-planの明示hashを検査し、上位DriverVersionと
非未来DriverDateを呼出側が明示した場合だけ新CATを作成・署名する。
SYSは既存署名と元CAT所属を確認してbyteコピーし、compileや再署名を行わない。
旧CATはコピーせず、新INF/SYSのCAT所属を標準signtoolで確認する。
source288b9315由来の固定テンプレートを逆置換し、名称以外の旧INF byte/文字列一致も確認する。
このrecipeの追加だけでは候補生成・署名を実行しない。WinPE alias/CAT/observer pin変更は別工程。

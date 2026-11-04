# MT8167 SoC温度：基盤実装（Windows未接続）

このディレクトリはWindows RT向けドライバの部品です。現時点ではKMDF、ACPI、
起動時devinfoの受渡し、クロック・AUXADCの排他、実際の停止・周波数制御への
接続がありません。`.sys`や実機試験可能なWindows構成ではなく、1.3 GHz動作の
熱保護にもなりません。実機で使うまで有効化しないでください。

`thermal.c`はLinux 4.4 MT8167の3センサーの校正式を使います。データは起動時の
供給者からdevinfo index 60–62を都度取得し、未校正・欠落は拒否します。Linuxが
使用する未校正の代用値を、校正済みの値として出力しません。`devinfo.c`は
bootloaderの `atag,devinfo` プロパティの長さとindexを確認する読み手ですが、
Windows UEFIからそのプロパティを取得する実装はまだありません。Linuxの
`get_devinfo_with_index` はこの配列を取得するAPIであり、レジスタ注釈だけを
根拠に直接eFuse MMIOを読めると断定していません。バックアップや端末固有値は
バイナリに含めません。

`hardware.c`はLinuxのthermal_fast_initと通常初期化をコールバックへ移した
未検証の候補です。読み書き開始前に、呼出し側はthermal/PTP、AUXADC、
APMIXED、infra reset、clockとファームウェア・全ドライバ間の排他を確立し、
`qualified`で証明する必要があります。これは共有資源の所有権を自動的に
確立しません。途中失敗後は状態が不定であり、再利用せず安全側へ移行します。
Windows上のACPIリソース、clock操作の実装、MMIOの安全性、AUXADCの完了・鮮度、
起動・スリープ・再開時の再初期化は未実装です。

候補 `a04_thermal_hw_protect` は校正済み閾値からLinuxと同じ候補レジスタを設定します。
SPMへの経路と停止動作は未確認で、実際の保護が働く証明ではありません。

`manager.c`はfreshな3点の最高温度、任意の閾値、ヒステリシス、タイムアウトを
扱います。未校正、I/O失敗、古い時刻、限界値超過ではHALTを要求します。
LIMIT/HALTは呼出し側への要求であり、実際に周波数を下げたり電源を切ったりする
バックエンドはまだありません。要求の失敗時はHALTを再要求しますが、それ自体が
停止の保証にはなりません。閾値は仮想のテスト入力で、製品の安全値ではありません。
ソフトウェア監視が停止した場合の独立したハードウェア熱保護も必要です。

`make test-thermal` は取得失敗、校正、変換、境界、排他拒否、疑似ADC、初期化
タイムアウト、鮮度、ヒステリシス、保護要求の失敗をホスト模擬で確認します。
実機の温度、正常なclock初期化、熱制限、RT動作、長期安全性は検証していません。

校正式とレジスタ手順の主な参照はMediaTek公開Linux 4.4の
`drivers/misc/mediatek/thermal/mt8167/mtk_ts_cpu.c`、同devinfo.c、
`arch/arm64/boot/dts/mediatek/mt8167.dtsi`。このディレクトリおよび対応テストは
GPL-2.0-onlyで、同梱COPYINGを適用します。リポジトリ全体のライセンスとは区別します。

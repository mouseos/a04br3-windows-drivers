# 同期PIO複数読取り試作

`A04_SYNC_PIO_MULTIREAD=1` は既定OFF。物理Storport、read-only、非ワーカー、DMA無効、非仮想の組合せだけを許可する。まだ `HwInitialize` は開始を拒否する。既存のboot/scratch/仮想診断経路とは併用できない。`tools/ci/build-msdc-multiread-suite.ps1` の `sync-pio-compile-only` はこの構成のビルドを準備する。

非仮想miniportの `HwStorStartIo` はStorportのStartIoロックにより逐次呼出しされ、同期処理はSRB完了後に後続ワーカーを残さない。Storportは `ScsiStopAdapter` の前に未完了要求を排出する。`IoReleaseRemoveLockAndWait` はPASSIVE_LEVELでしか呼べないため、DIRQLの停止コールバックの解決策にはしない。デバイスextensionを独自のカウンタだけで保持できるとも仮定しない。[HwStorStartIo](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/nc-storport-hw_startio)、[SCSI_ADAPTER_CONTROL_TYPE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/storport/ne-storport-scsi_adapter_control_type)、[IoReleaseRemoveLockAndWait](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-ioreleaseremovelockandwait)。

PIO部品はDMA有効中、PIO無効、関連割込み有効中に開始しない。最大8セクタをCMD23で制限しCMD18で読取り、正常完了後だけ専用ステージ領域から要求バッファへコピーする。1セクタはCMD17。データ異常やタイムアウトでは部分データを公開せず、複数読取りならCMD12/R1bとDAT0を待ち、停止失敗は別エラーにする。書込みコマンドもDMAレジスタも発行しない。CMD23とPIOの参照元は機種XNU `port/xnu/kernel/bsd/dev/a04br3_msdc.c`、`unixnu/UniXNU/drivers/NativeMsdc/NativeMsdc.cpp`。Linux v6.6 `drivers/mmc/host/mtk-sd.c` は要求のDMA mappingと停止を管理し、CMD23が使えない複数要求には停止コマンドを使う。Windows用の同期やPnPをそれらが保証するわけではない。

ホスト模擬はコマンド順、境界、DMA活動中の拒否、タイムアウト、CRC、停止失敗と部分結果非公開を確認する。ARM Thumb COFFコンパイルは部品の構文・ABI確認に限る。

**解除条件未達:** 既存の物理Storport経路では過去にE6/1F/04が発生し、SRBマッピングとalignmentの原因が未確定。同期PIOでも物理除去に伴うMMIO abortは防げず、タイムアウトの最悪時間は高IRQL滞在として実測・削減が必要。Windows 8.1 WDKの新構成のリンク、PnP・power・復帰と実機読取り、独立した正解データ照合も未検証。非同期DMAの旧経路は残っており、書込みにこの設計を転用していない。これらを理由に開始拒否は解除しない。

## 停止と要求の同期

同期PIO構成はStartIo内で状態の受付権を取得し、その呼出し内で処理と完了を行う。StopAdapter、adapter対象の停止・除去PnP、バスリセットは受付を閉じる。ポーリング中は閉鎖を確認して中断し、観測後のMMIOを抑止する。既に転送中の命令と閉鎖通知を原子的にできるわけではない。中断時はCMD12を含む追加のMMIOを避け、部分データを返さない。read-only構成のFLUSH/SHUTDOWNは未書込みとして完了する。再開時のハードウェア初期化は未実装なのでRestartは拒否する。バスリセットの成功は報告しない。

これはStorport管理の寿命を延長する機構ではない。StopAdapterは正常時に未完了SRBがないというポート契約に依存する。突発除去と進行中の1回のMMIO、バス例外、デバイス再初期化を解決したものではなく、開始拒否を維持する。直前コミット 4bd4de0 の４構成はWDK 8.1 ARM32リンク・CAT生成に成功したが、この追加差分は別途ビルドが必要。

同期PIOの各MMIOは状態ワードの単一アクセス所有権を取得してから行い、完了後に返す。閉鎖と取得は原子的で、閉鎖後の新規アクセスは拒否する。既に取得済みのアクセスが残る間は状態上の排出完了を認めない。これは実ハードウェアがアクセス命令の前または最中に消えることを防げず、Storport所有資源の解放も止めない。例外でアクセスから復帰しない場合に所有権を解放したことにしない。通常停止はStorportのSRB排出契約、突発除去の実際のハードウェア保証は別途必要。電源復帰はクロック・電源・カード再初期化の実装がないため引き続き拒否する。

## マッピングと再開照合部品

同期PIO構成では `StorPortGetDeviceBase` が失敗した場合、管理外の `MmMapIoSpace` にフォールバックせず開始を拒否する。取得後に設定を拒否する場合は `StorPortFreeDeviceBase` を呼ぶ。この構成ではUART直接マッピングとドライバ全体のバグチェックコールバック登録もしない。Storportのマッピングを使うこと自体は、アクセス中の物理消失からの復旧を保証しない。

`resume_check.c` は再初期化後の観測値を既知状態と照合する部品であり、現時点ではハードウェアに接続されていない。電源・クロックの所有、DMA停止、PIO・割込み状態、カードの転送状態、CID・容量・RCA、バス幅、ユーザー領域とキャッシュ無効を保守的に検査する。合格だけで復帰やリソース所有を証明しない。信頼できる同一電源サイクルの世代・CID等の取得先、電源とクロック・ピン・カードの初期化主体、排他・再開順序は未実装。値を仮定で供給せず、再開と起動の拒否を維持する。

## 再開後の状態取得試作

`resume_capture.c` は排他所有と電源・クロック・ピンの有効性を保証する外部主体から、世代付きの許可コールバックを受けて初めて実行する。PIO・安定クロック・DMA停止・割込みマスクを確認し、選択済みRCAに対するCMD13、CMD10（CID）、CMD8（EXT_CSD）、再度のCMD13から `resume_check.c` 用の観測値を作る。途中で許可を失えば追加コマンドを送らず、中途半端な観測値を公開しない。CIDはメモリ上でのみ扱い、ログや配布物へ埋め込まない。この部品は実際の電源所有者、WindowsのPnP、再開処理のいずれにも接続されていない。コールバックが真を返すこと自体は物理的な電源や資源寿命の証明ではなく、再開ゲートを開く根拠ではない。

機種DTSではMSDC0は8-bit eMMCで、VEMC3V3をVMMC、常時有効のVIO18をVQMMCに指定する。Linuxはclock ungate後に保存レジスタを復元するが、必要なレギュレータ・ピン・時計の所有者と排他も使う。機種UEFIのCMD0/CMD1/CMD2/CMD3/CMD7等は一連のコールド初期化で、Windows側から安全に実行できるという証拠ではない。実際の電源・ピン・クロック復帰、保存状態の真正性、カード再初期化、物理除去とMMIOの競合は未解決。安全ゲートは維持する。

## 共通資源の所有者に接続するための機種部品

`board_owner.c` は実験的なPASSIVE_LEVEL用の部品で、電源・TOPCKGEN・GPIOの排他的な長期リースを提供する親を要求する。既存Windowsにはこの親がなく、Storport・PnP・ACPIに接続していない。親はファームウェアとタッチ、ボタン、他ドライバを含む共有資源への排他、停止中の所有権、故障後の回収を保証する必要がある。呼出側の真偽値を物理的な所有権の証明とみなさない。

共通PWRAP所有者には固定されたVEMC enable、VEMC selector、VIO18 enableの３レジスタの読取りを追加し、ボタン読取りと同じロックで一組として扱う。どれか失敗した場合は結果を公開しない。電源電圧・有効状態を書き換えず、VIO18を無効化しない。EN/QIはレジスタ表示であり実電圧の実測ではない。

機種DTSでMSDC0に割り当てられたGPIO110〜120のmode 1を親が確認した後、TOPCKGENのMSDC0 source gateをCLR、inverted infra gateをSETし、それぞれ状態を読み直す。既に有効なら書かない。途中で所有権・アクセス・読戻しが失敗したら状態をpoisonしてリースを保持し、勝手な巻戻しや再試行をしない。正常時もリースは保持され、親がI/O・DMA・MMIOの停止を確認してはじめて返却できる。ピンのpull・drive設定、電源電圧の実測、CLK muxと周波数、カード初期化、故障後のリソース回収はいずれも未実装であり、処理の成功は再開許可を意味しない。

参照した機種Linux正本は [a04br3-linux-drivers 3d2c74e](https://github.com/mouseos/a04br3-linux-drivers/tree/3d2c74e252d6f34a5980deda96bdc04064108d74) の `clock/a04br3-mt8167-msdc-clock.c` と `pmic/mt6392-regulator.c`、機種DTS `mmc0default`。Linux固有ドライバはGPL-2.0-onlyで、ここでは確認したレジスタの位置・ビット・極性を用いた独立実装とする。実機検証、Windowsの資源共有と再初期化は未完了。開始・再開ゲートは維持する。

## コールド初期化・ピンモードの試作

`cold_init.c` は外部の親がレール・ピン・リセットと400 kHz以下の1-bitホストを準備し、処理終了まで排他的に保持することを前提とするPASSIVE_LEVEL部品。CMD0、CMD1（sector mode）、CMD2、CMD3、CMD9、CMD7、CMD16を送り、既存の状態取得部品でCMD13/CMD10/CMD8/CMD13を実行してCIDを照合する。CMD6とカードデータ書込みは行わない。タイムアウト・取消し・状態不一致の後は自動再試行せず、部分結果を公開しない。コマンド順は機種UEFIのSdCardBlockIoDxeに基づく。実電源・クロック・ピン設定の実装や実機動作を示さず、Storportから呼ばれない。

`pinmux.c` は親がGPIO、電源とコントローラを排他し静止・給電済みである場合だけ、GPIO110〜120のmode 1を共有ロック付きmasked updateで設定・読戻しする。offset 0x460/0x470/0x480と3-bit×5ピンの配置はLinux v6.6のMT8167 pinctrl、ピン番号は機種DTSに基づく。途中失敗はpoisonとしロールバックしない。pull、drive strength、入力有効や電圧は未設定。親のコールバックとWindows PnPの接続も未実装。両部品の成功だけでは起動・再開を許可しない。

カード初期化と状態取得では各 MMIO 前に許可を確認し、ARG 書込みと CMD 発行の
間に停止された場合はコマンドを送らない。各許可確認位置で停止を注入するホスト試験を
含む。これは確認と MMIO 命令の間の物理除去を防ぐ仕組みではない。取得終了時にも
PIO・clock stable・bus width・DMA idle・割込み mask を読み直す。controller CKSTB と
ソフトウェア許可だけからレール所有や共有クロックの安定を証明せず、これらの観測値は
0 のまま返す。実際の電源所有者と Storport 再開への接続は引き続き未実装。

状態取得のアクセス許可は実レールの所有や供給電圧を証明しないため、
`a04_resume_capture` は `power_owned` と `clock_stable` を 0 のまま返す。
controller の CKSTB は共通 source clock の所有・安定性を証明しない。コマンド終了後に
コントローラの PIO・clock stable・バス幅・DMA・割込みマスクを再確認し、
失敗時は出力を変更しない。この観測だけでは `a04_resume_validate` は成功せず、
実電源の所有と世代を別途確立するまで起動・復帰ゲートを解除できない。

機種固有の [Linux MT6392 regulator](https://github.com/mouseos/a04br3-linux-drivers/blob/68f064d50de4568049e09e5990a6e21cfa67c1b6/pmic/mt6392-regulator.c)
では VEMC3V3 の 0x052e bit 7 の選択値 0/1 がそれぞれ 3.0/3.3 V、
0x0508 bit 14 が enable、bit 15 が QI、VIO18 は 0x0556 bit 14 enable、
bit 15 QI の固定 1.8 V と記述される。機種 DTS の VEMC 範囲は 3.0–3.3 V、
VIO18 は常時有効、両 rail の enable-ramp-delay は 264 µs である。これらは
Linux の記述とレジスタ観測であり、実電圧・電源の持続・Windows での排他
所有の証拠ではない。VIO18 の無効化、VEMC の切替や再有効化は、他クライアント
と card の状態・保護順序を確立するまで行わない。

`a04_emmc_prepare` は未接続の一括復帰試作で、同一世代の排他 lease の下に
VEMC 再有効化、MSDC pin mode、clock 選択・ゲートの readback、機種側の reset／
400 kHz host 設定、CMD0/1/2/3/9/7/16 と card 状態取得を順にまとめる。
途中失敗では lease を解放せず poison にし、結果を公開しない。最終観測でも
実レール所有・共有 clock 安定の証拠はゼロのため、boot／resume の許可にはならない。
この関数は PASSIVE_LEVEL を保つロックと非取消 lease を必要とし、現行親の
FAST_MUTEX は IRQL を上げるためそのまま使えない。親、Storport、IRQ や実電源
への配線は未実装であり、ホスト模擬試験はハードウェア動作を保証しない。

`host_prepare.c` は `emmc_prepare.c` の既定外の具体的なホスト準備経路で、DMA idle を検査した後、割込みをマスクし、ホストと FIFO をリセットする。親が保証するクロック上限から 260 kHz 以下となる分周値を計算し、親が排他的に所有するゲート操作で変更して安定状態と１ビット PIO を読戻す。ホスト試験で各段階の失敗と途中停止を検査する。レール・ピン・クロック・ホスト・カードのモック一連試験にも接続した。

この部品の上限周波数、電源とゲートの所有権、バスへの安全なアクセスは呼出し元の責任であり、現在の WDF 親は実装していない。Windows 親や Storport に接続されたこと、物理除去中の MMIO が安全であること、また実機で初期化が動作することを示さない。D0 と eMMC の起動・再開ゲートは維持する。

ホスト準備前に同一 lease 下で TOPCKGEN の機種 UEFI が使用する eMMC mux 値を照合し、ホスト準備の前後に呼出し元が選択クロックと周波数上限を検証するコールバックを要求する。mux 値は PLL の周波数・安定性・所有権の証明ではない。未確認の Windows 親側から値を決め打ちして成功を返してはならない。

共有資源試作を読取り性能ブランチへ統合し、host準備の各MMIO読取り・書込み間と
最終照合中に所有権を再確認する。レジスタが全ビット1を返した場合も準備成功に
しない。各確認位置で所有権喪失を注入するホスト試験を追加した。ただし確認後の
単一MMIO中の物理消失は防げず、WDF親D0・実リース・Storport DMAとの接続は
引き続き未実装。物理miniportの開始拒否を解除する根拠にはならない。

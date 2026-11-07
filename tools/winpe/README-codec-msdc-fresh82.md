# TAB-A04-BR3 codec1026 / MSDC quiet0.0.1.8 一括延期更新

固定profile `codec1026-msdc-quiet-fresh82` は既存Engine媒体/サービスの明示opt-in。
親1015からcapture1026への選択統合済み署名tupleと、配置済みf8f54c9を基点とした
ログ限定のMSDC quiet1.8だけを使用する。他の候補や旧capture親を探索・選択しない。

更新対象は ACPI\BEN0001 / Mt8167Shared、MT8167\AFE / Mt8167Audio、
ACPI\MOU0001 / a04msdcboot。親の既存32flags・RTC borrow=1は保持し、capture=1だけ追加。
移行前Audioはendpoint1007。USB1005は44899b9の配置SYSを保持し、監査用再署名版に替えない。
MT6630 Wi-Fi1012、MOU0007 CPU DOD、MOU0006 GE8300Render3.9、旧GE8300 provider、TOPRGU、
GE8300 firmware/UMDを固定hashと実service/versionで保持する。Code43をCode0に置換して扱わない。
UMDの配置先はSystem32、SYS/FWはSystem32/drivers。GPU sourceやドライバsourceは編集しない。

`codec_msdc_transaction.py --draft` は署名packageのsource/manifest/artifact/cert/INFと
1015 flagsを確認したdraft JSONだけを出す。backup fieldsはnullが必須。実行可能headerは出さない。
通常のpolicy入口はfresh82の完了manifestと全file hash/bytesを確認し、配置SYSを照合する。
`private-windows-rt-prebackup-run82/manifest.json`以外のbackupは拒否する。旧SYSTEMへfallbackしない。
FW/UMDがbackupへ未収録でも、runtimeの固定hash gateと旧file保存を省略しない。

既存 `build-mt8167-shared-io-update-media.py` に同mapを渡す。Engine modeの既存引数と
正しい使用済みEngine媒体manifestを明示し、既存のrearm plan→native helper→media生成を使う。
保持対象へのDISM、installed driver SYS直接copy、親のlive restartは行わない。
`prepare-codec-msdc-engine.cmd.in`は新service/clientと3packageをEngine専用領域へ置く。
既存marker/log/serviceの保存とnative SYSTEMの7値限定監査は元のEngine frameworkを保持する。

WDK入口 `tools/ci/build-codec-msdc-fresh82.ps1` はfresh82結合mapだけを受け付ける。
同じ既存runner/SDK/署名鍵でservice TUだけをcompileし、既存driver/clientを再buildしない。
実SYS/INFのCATmembershipを再照合し、サービス署名packageを生成する。

通常OS初回は全3package hash/署名/CATmembership、旧SYS/version/date、保持deviceと
MSDC BOOT_START/SCSI Miniportを事前確認。全て通過後、旧SYS/FW/UMDを
`System32/MT8167EngineProbe/installed-before/`へCREATE_NEWで保存しhash/flushを確認する。
その後だけ標準SetupAPI/DIF_INSTALLDEVICEを各1回、DI_DONOTCALLCONFIGMG/DI_NEEDREBOOTで呼ぶ。
自分を無効化し、既存waiterによるplanned rebootは1回。部分失敗時も元のerrorを保持する。
保存物は自動restore・自動retryを行わない。最終選択/hash/CM観測と実bootを別に判定する。

次bootは新版の選択/version/date/実SYSと保持hashを照合後、既存署名KS capture clientを
引数なし・120秒上限で1回だけ実行する。WLAN scan、描画、render clientは起動しない。
capture.logとstdoutには48kHz stereo16bitの2 passのCUR/IRQ変化と停止後PCM aggregateを残す。
生マイクPCMを保存せず、音響品質・標準Audio Engine・full-duplex・latency確定は主張しない。

MSDCは新版SYSの普通のファイル読み出し2 passを実行し、bytes/digest/elapsed/errorを記録する。
OS cacheを迂回しないので物理eMMC throughputや速度改善の測定ではない。エラー注入もしない。
正常UART抑制・error/init/checkpoint/DMA poll保持は既存quiet source検証と主担当の保存UARTを
別々に照合する。UART文字混線・タグ不存在だけで保持成功とはしない。

`verify-codec-msdc-evidence.py` は適用後backupと主担当が指定した既存UARTのboot範囲だけを読む。
capture/DMA/IRQ aggregate gate、普通のfile read結果、全SYS/保持file hashを個別に出力する。
実機を操作せず、未回収・未検証を成功扱いしない。capture optionalログの回収は既存
`capture-windows-prebackup.py --engine-probe-state`の固定候補に追加した。

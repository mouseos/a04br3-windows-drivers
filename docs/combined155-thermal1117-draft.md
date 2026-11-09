# run155：actual154基点・343＋matched thermal1117案（draft）

sourceは凍結153最終beb3edccから独立。153の媒体は変更しない。
`tools/recovery/prepare-combined155-draft.py --workspace ROOT --out NEW_ROOT_LOCAL_OUTPUT` はcontract-DRAFT.jsonだけを生成する。正式package-mapではなくbuilder/native armへ入力してはいけない。reject_arm()は拒否する。

## 実基点と範囲

fresh154 manifest a26e82c937733617d932bb92201a593885575f27e89f26791612f791ca17b8da、Recovery9d795d5d-8d58-47a2-b208-e5c0149ecd19。原41の全hash/size、postとinit-receiptのsource manifest/SYSTEM trio/clean load-unload-restore/未変更/ModuleIdentity342を結合する。

153はterminal21/finalization0/restart1、GPU Code37。PB220なしでclient未実行、Clear実行前。追加cloneでReachedMask7、DriverEntry0/Contract0/DxgkInitialize C000003B。AddDevice以降の保存0は今回成功ではない。PE差を単独root原因と断定しない。

155案は親1117＋boot2e6＋GPU343。DIF parent/index6とrender/index10のみ（mask0x440）、予定restart1。boot_write_required=true、leaf1.10および他actualを保持する。親1117 b2c1a313/SYSbbced952＋FW b4ce1992/boot2e6は同update必須。ATCV1/ATCO1のmixed拒否を保持する。

343はmain報告の同80objects/MSVC14.29純relink、compile0、branch work/ge8300-msvc-relink343/323b3207、INF0.0.3.43。runtime BuildIDはGPU342-tq-clear-v1、compiled source59ded879、sourcebase eb2bc820のまま。BuildIDをGPU343に改名しない。342/343の識別には実SYS hashと選択INFを併用する。343 artifactとmain sealは未結合null、通知前はdraft/no arm。

## 診断とobserver

同bootでbuiltin温度0x226810/28Bを1回→status542c7393 --status/0x226814/64Bをphase0で1回（10秒、測温失敗時も）→GPU9934 --hardware=PowerVRをphase1最後1回（900秒）。status exit0と温度成功を分離する。stdout logs/thermal_status.stdout.txt、binaryはclient cwd/thermal-ioctl-status-v1.bin。音声/WLAN/BT/benchmark/他機能再試験0、BT初回登録0。既存SystemRequiredと12target metadata観測を保持する。

install900秒、boot待ち180秒、最初phase1 PB286で診断180秒へ一度更新、最初の現GPU PB target0/event0x220で900秒へ一度更新。繰返しイベントで延長しない。診断最大1080秒。GPU未開始なら900秒延長しない。新boot起動/restart/RecoveryのUART確認はmainのみ。

## 追加post helper（SCM再compile不要）

read-gpu-native-checkpoint.ps1をread-combined152-post.ps1の既存cloneセッションへ接続した。11init status＋ReachedMask＋DdiVersion＋RegistryPathの14fixedfieldsを回収する。ReachedMaskのstage bitが立たない保存値をstale候補として出力し成功推論しない。ModuleIdentity144Bは既存readerの別欄で保持。same-capture manifest/trioに結合し、別PC/別boot値との比較で現在成功を主張しない。このreader本体の155用profile/client数への正式化は343 seal結合時に行う。

## 正式化の残条件

343の完成署名tuple/main seal、Recovery未変更証拠、actual154の全選択driver、Service152を含む最新停止values、mutableログsnapshotを結合する。旧152を154と偽装しない。旧boot baselineとupdate後新bootのhashは分離する。immutable hive/driver guardとmutableログ保存prefix比較の分離を保持。

validator/native preparation/post156 reader/policy/headerの最終hashを一致させ、既closureでservice1TUのみ増分build/signしてmain独立sealへ返す。署名済parent/FW/GPU objects、SDK/libsを再compile/setupしない。このdraftはservice/native/mediaを生成しない。温度/新boot復旧/343開始/描画成功はすべてmain実機結果待ち。

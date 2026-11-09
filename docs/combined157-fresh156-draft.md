# run157／実fresh156基点 GPU344-only非arm模板

実装入口 `tools/recovery/prepare-combined157-draft.py --workspace ROOT --out NEW_ROOT_LOCAL_OUTPUT`。

原41のhash/size、Recovery1fc923b6、clean native source trio、ReachedMask7ff/11init0、actual343 SYS e884、actual親1117 SYS bbced、thermal1.10/他保持SYSを照合する。旧service154は実nativeのStart4＋5値。旧20値もactualと照合し、18 full service fingerprints＋25 raw valuesとして保持する。未取得hashを作らない。

実155回収69logsをfresh156 baselineへ結合し、旧67は全byte不変・新2だけ追加と確認。mutableログをimmutable admissionへ戻さず、その場で削除/上書きしない。

main155報告は温度first sample44.9/41.1/41.7℃・phase7全0成功。GPU343は全init成功したがPID2536 firstCopyでBOOT_REASON9予定外reset。描画全体成功としない。157は親1117/boot2e6/thermal1.10を保持、GPU344だけDIF(mask0x400)・予定restart1。温度/status/Audio/WLAN/BT/benchmark/他機能再試験0、BT2014候補も混載しない。

GPU344は既存PASSIVE registry進行記録の永続化4TU差分で、payload変更やGPU根因修正ではない。採用recipeは0feb67ff、registry helper guard差分17af96fe（PASSIVEに加えKeAreAllApcsDisabled()ならskip）。正本はport/xnu/out/windows-rt-durable-progress344-guarded。初版81b49ee9署名物は非採用履歴・未実機。未完成artifact/main seal/BuildIDはnullとしarm不可。正式化は最終manifest/main seal後。

GPU9934 --hardware=PowerVRだけをphase1最後1回/900秒。157では**client index0 / PB target0,event0x220**（155のindex1から戻る）。install900、boot180→最初phase1 PB286から180→現GPU PB220から900秒へ各一度だけ更新。繰返しで延長しない。

post158では14fixed initfields/ReachedMask/ModuleIdentity144B/actual SYS+selectedINFと、344最終sourceが定義するdurable進行値を同captureで回収する。別boot PCとの比較をしない。失敗receiptもarchive/get後に判定するmain修正を継承。

署名service/native/mediaは生成しない。既344/親/FW/thermalを再buildせず、採用時は既cacheによるservice1TUだけ。Recovery未変更再確認/実機/boot復旧操作はmain所有。

## guarded344 seal結合後の正式入口

正式入力prepare-peripheral-followup156.py / combined156_policy.py、署名tools/ci/build-combined156-service.ps1、native clone専用prepare-offline-system-service.ps1、post158 reader read-combined156-post.ps1＋既存read-gpu-native-checkpoint.ps1。媒体--combined156-profile。
main sealのca9d2687 manifest/SYS2090a01f、source0feb67ff、runtime BuildID GPU344-durable-progress-v1、追加import KeAreAllApcsDisabledだけの実RT81 export照合を固定。親1117/thermal1.10/boot2e6を保持しGPU344だけDIF。18full＋25raw values、原41、69logs、UMD/FW actual155回収byteを照合する。GPU payloadの修正とは呼ばない。APC guardは予防的制約で、ZwFlushKeyの公開PASSIVE条件やPC一点から前回reset原因を証明したとは扱わない。
旧draft generatorは非arm入口として履歴保持し、正式map/header/service/native/mediaは完成artifactから別生成する。

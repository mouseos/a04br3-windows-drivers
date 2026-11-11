# 同captureのWiFi/keypad署名更新

既shared200のpackage検査・inventory_contract・C transactionを使用する。
`peripheral-signed-updates-accepted.json`はWiFi1033 **public cold signed** と
keypad14の実manifest/main review/全trioを固定する。増分1033と混同しない。
keypadの原schema/deployable=falseやsplit source出自を上書きしない。

候補準備は従来どおり `--workspace <root> --out <新出力>`。
`--baseline-map <map>`はactual nativeの全service/全named値を検査する。
主担当のbaseline mapを`baseline_inputs` path/SHAで結合したfinalize mapに、
同captureのsupplement/mutable、13role actual_package_rows、最後のboot seal/resultと
no-intervening-write報告、install_targets=[wifi,keypad]を指定する。null/SHA未確定は拒否。
旧captureへの追加loadやchild service_rootsの合成は行わない。

```sh
python3 -B tools/recovery/peripheral-signed-updates.py --workspace "$ROOT" \
  --map <実finalize-map> --out <新出力> --finalize
```

profile名は `combined-features-fresh<N>-signed-peripherals-v1`、serviceは
`MT8167CombinedFollowup<N>`。Nは204..999、run=N+1/post=N+2。
更新wifi/keypadのmask136、予定restart1、GPU client起動0、正常terminal後Windows保持。
GPU379の現搭載SYS・UMD・FW/transport・他11role・clientファイルはactualの実byte保持。
GPU候補待ちを条件にせず、float/unorm新DLLも採用しない。USBは別工程。
module identityの実文字列をpackage番号から生成しない。bootは保存済seal/result出自であり、
当該captureで物理readしたとは主張しない。

正式source入口:

- SCM: `tools/ci/build-combined200-service.ps1 -SignedPeripherals -CaptureNumber N
  -Mode Service -Workspace <root> -PackageMap <map> -OutputDirectory <新出力>`
  と既KitHeaderRoot/LibraryRoot/SigntoolPath。必要SCM1TUのみ、driver/SDK再buildなし。
- native: 既 `prepare-offline-system-service.ps1` へ生成preservation plan/hashと
  新service名を渡す。元SYSTEM trioと同native receipt、全service/named値の一致を要求。
  新serviceの既7values以外を書き換えない。実行はmainの調整後。
- media: 既 `build-private-peripheral-batch-media.py --signed-peripheral-profile` と
  上記map、同sourceの署名SCM/native、既media入力。source/header/token一致を再生成検証。
- host: 既 `run-gpu368-fresh200.py --signed-peripheral-capture N`。
  seal schemaは `SIGNED_PERIPHERALS_RUN<N+1>_MAIN_SEAL_V1`、
  diagnostic_client_count=0、GPU_client_index/globaldiag_client_index=null。
  generator_path/map/全input_pins、controller_engine/collectorをsealへ固定。
  pre-latch stage検査、同boot/run/PID/flush0・予定restartの証拠は旧経路同様に必須。
  `--validate-only`からmainが媒体/sealを検証し、実機・別collect-onlyはmain担当。

55service/333named値等の件数はactual契約から導出し固定番号のadapterを増やさない。
service全体の免除なし。DIF後root遷移を推測許可しない。keypad14は既schema/epoch/
80B shapeとfirst-only B25..31のwriter boundsを追加opt-inで検査し、旧B24上限は既定維持。
このsource/profile生成は、Windows SCM compile/native patch/media/実入力・接続成功の証明ではない。

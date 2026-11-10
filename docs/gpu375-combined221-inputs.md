# fresh221/run222/post223: client375と一括更新

既prepare-gpu375-client-inputs.pyのmask0/diagnostic-only/restart0入口は変更しない。
新profileはcombined-features-fresh221-gpu376-battery18-wifi1031-client375-v1。
service/dataは新MT8167CombinedFollowup221を共有し、run222の同じonce-only latchを使う。
旧mask0候補とcombined候補を両方実行する入口ではない。

## 正式tuple

既Cのrole indexとinstall_orderをそのまま使う。

| role | index | bit | candidate |
| --- | --- | --- | --- |
| display | 5 | 32 | transport372 |
| render | 10 | 1024 | render376 |
| wifi | 3 | 8 | MT6630 1031 |
| battery | 8 | 256 | LC709203F 1.8 |

DIF順はdisplay→render→wifi→battery、合計1320/0x528。
UMD/FWは実372packageのbyteを使い、actual221の旧hashはbaseline witnessとして保持する。
render371を新candidate tupleへ混ぜない。実旧SYS/version/dateを新値へ書き換えてbaselineと称さない。
新boot/GPT書込み0、予定restart1、Present/helper各1、terminal Windows保持、collect-only再配備禁止。
電池Code10をready成功へ変換しない。既C readiness gateは変更しない。

## exact manifest受理

source管理のtools/recovery/gpu375-combined-accepted.jsonに正式原本ref/schema/sourceを登録する。
受理statusは全tupleと個別main reviewが確定した時だけactual-signed-manifests-reviewedへ変える。

現在の実入力：

- render376 manifest SHA7813356ceea205e536bccaa3b82fb077ac2e20a8712302468637aa074052644b。
  source2f63b0e6b7e4466c89dbc9c48d0f005d91f9069d、署名recipe48b3fd2a001022e1fcd34d17593548875799361d。
  署名schema ge8300-gdi376-render-sign-only-v1を原本のまま読む。
- retained372 manifest SHAe987826bdc5a9a8085c31ebeccf6c0680e96cd8bb7ef5ad56748869bc41bccfc。
  transport372/UMD372/FWの実原本。376出力の保持byteと照合する。
- battery1.8 manifest SHA7adc56c789b7b49c3391b090cd60b258f1b5e970987bba0a7cb2ed355f53f3a7、
  source049e8937d05b5fe747c64bd85f6b0585286fdd90、INF11/10/2026,0.0.1.8。
  compile source32dfとINF/source049eを混同しない。
- WiFi1031は未確定のためnull。仮SHA/schema/receiptを作らず、生成は拒否する。

GPUと電池の個別main reviewをそのままpinする。新しい総合main reviewは不要。
flat manifestへ丸写しして新compiled sourceと偽るadapterは使わない。
source aliasはprepare-peripheral-followup184.pyでexact372/376 native schemaだけを認める。

入力shape：

- schema: GPU375_COMBINED_PREPARE_INPUTS_V1 / mode: gpu376-battery18-wifi1031-client375
- hardware_owner: main / capture_number:221 / run:222 / post_capture:223
- client_map: 実375 mask0 mapのpath/SHA（既native221/正式pair/同Recovery補足を再用）。
- candidate_packages: display/render/wifi/batteryの既package行。
- main_execution_approved: 実main承認のbool（finalize時trueを要求し、自動付与しない）。
- battery_checkpoint: nullまたは省略。221への再load補足は要求しない。

## 明示CLI

入力準備だけ：

```sh
python3 -B tools/recovery/prepare-gpu375-combined-inputs.py \
  --workspace "$TASK_ROOT" --inputs "$ACTUAL_COMBINED_INPUTS" \
  --out "$NEW_ROOT_LOCAL_OUTPUT" --combined-update --prepare-only
```

全signed pin確定後のpolicy/header生成は同じ入口の--prepare-onlyを--finalizeへ変更する。
SCMは既build-combined200-service.ps1へ-CaptureNumber 221 -Gpu375Combined -Mode Service。
既1TU/鍵/SDKを再用する。これはsource接続の説明であり、今回build/signの実行はしていない。
private nativeは既prepare-offline-system-service.ps1が新profile/実221/50svc298values/4role tupleを検査。
mediaは既build-private-peripheral-batch-media.pyの--gpu375-combined-profile。
collectorはGPU375_COMBINED_RUN222_MAIN_SEAL_V1をcombined-run222/new221へ結合する。
controller入口はtools/recovery/run-gpu375-combined-fresh221.pyで、既Observationとengineを再用する。

SCM/native/media/controller全入口で未確定WiFi tupleを通さない。
Windows PS5 parserの実行はこのsource作業では行っていない。変更host PS1はUTF8 BOMで保存。

## post223での電池exact2値

公開reader029626f9の実装120e4cebを限定統合した。
既read-fresh200-native-inventory.ps1に次の**実223の同一load**でのみ
-TimingCaptureNumber 223 -IncludeBatteryFirstFailureを付ける。
元221 single-loadは成功済みで、再loadしない。

固定key: ControlSet001\Services\LC709203F\Parameters\NativeCheckpoint。
LoadEpochV1(REG_BINARY16B)とFirstFailureV1(REG_BINARY96B)だけを読む。
receipt.battery_first_failureにpath/present/same_loaded_root/values/epoch_matches/schema_validを保存する。
missing/stale/schema不一致を成功にせず、同receiptのselected INF/SYSとbootに結合する。
補足はcore inventoryの後、既unload/privilege restoreの前に実行し、loadを増やさない。
新post223 capture/receiptのpath/hashはまだ無いので生成物に作らない。

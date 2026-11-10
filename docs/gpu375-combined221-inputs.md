# fresh221/run222/post223: client375と一括更新の入力準備

既 `prepare-gpu375-client-inputs.py` のmask0/diagnostic-only/restart0入口は変更しない。
新入口 `tools/recovery/prepare-gpu375-combined-inputs.py` は明示
`--combined-update --prepare-only` 専用。既inventory221とclient375 mapをRAM上で再検証し、
GPU376（372基点）/LC709203F 1.8/MT6630 1031のcandidate package行とDIF対象だけを準備する。
新inventory、RegLoadKey、SCM、署名、native patch、媒体、端末操作は行わない。

## 未確定入力と受理条件

`tools/recovery/gpu375-combined-accepted.json` の3 manifestは現在null。
これは未取得の明示でありpinの代用品ではない。現在は生成を必ず拒否する。
実署名とmain reviewが揃った後、各枠へ次を**実manifestそのまま**登録する別source commitが必要。

- `manifest`: root内原本pathと実SHA256。
- `schema`: 原本のschema、`source_field`: 原本source出自field、`source_commit`: 実full commit。
- `versions` / `dates`: role別の実INF tuple。render=0.0.3.76、battery=0.0.1.8、wifi=2026.11.11.1031以外は拒否。
- lock直下 `main_package_review`: mainが独立照合した実reportのpath/SHA。
- lock直下 `display_update`: 実376 archive/memberと署名manifestで確定するbool。
- 確定後のみ `status=actual-signed-manifests-reviewed`。

未取得schemaを推測してadapterを作らない。既package検証器が原本source形式を読めなければ、
実schema到着後にそのexact pin専用の狭いadapterを追加する。flat manifestに丸写しして
compiled sourceやretained sourceを偽装しない。main reviewには実Valid署名・CAT membership・
全leaf・既372由来と変更TU・未実機検証を残し、旧proofを新実行の証明にしない。

入力JSONのshape（値やpath/SHAを埋めた仮JSONは作らない）：

- schema: GPU375_COMBINED_PREPARE_INPUTS_V1
- mode: gpu376-battery18-wifi1031-client375
- hardware_owner: main / capture_number:221 / run:222 / post_capture:223
- client_map: 実375 mask0 mapのpath/SHA（native221/正式client pair/同Recovery補足の既検証を再用）。
- main_package_review: lockと完全一致する実report ref。
- candidate_packages: render/battery/wifi（display_update=trueならdisplayも）の既package行。
- battery_checkpoint: 任意の実exact2値補足ref。未回収ならnull、成功を合成しない。

電池read対象はHerschel共有のControlSet001実key
`Services\LC709203F\Parameters\NativeCheckpoint` のLoadEpochV1(REG_BINARY 16B)と
FirstFailureV1(REG_BINARY 96B)だけ。新全inventoryを要求しない。
補足結合は同Recovery/SYSTEM trioを確認し、旧native221の原本・hash・集合を変更しない。
補足のreceipt正規shapeはHerschelの公開実装と合意してから入力する。

## mainが実入力確定後に使う準備コマンド

```sh
python3 -B tools/recovery/prepare-gpu375-combined-inputs.py \
  --workspace "$TASK_ROOT" --inputs "$ACTUAL_COMBINED_INPUTS" \
  --out "$NEW_ROOT_LOCAL_OUTPUT" --combined-update --prepare-only
```

出力はcombined-candidate-inputs.jsonのみ。SCM/header/native/mediaへの実行許可にはならない。
DIF順は既Cのinstall_orderをfilterしたdisplay→render→wifi→battery。
transport保持ならrender/wifi/battery mask1288(0x508)、displayも更新なら1320(0x528)。
selected baselineは**実旧hash/version/dateのまま**、更新SYSをPreservedSYSから除く対象名を示す。
新service221、既boot98de/GPT書込み0、予定restart1、Present/helper各1、terminal Windows保持、
collect-only再実行禁止は後段既controller契約を維持する。

このcommitは入力準備専用。既SCM/native/media/controller入口へcombined実行を広げていない。
正式生成へ進む際は実tuple確定後にその明示入口を接続・reviewし、既mask0の扱いを緩めない。

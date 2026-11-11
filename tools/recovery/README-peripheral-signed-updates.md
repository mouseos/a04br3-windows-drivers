# actual capture後のWiFi1033 cold/keypad14候補準備

`peripheral-signed-updates.py` は既shared200のpackage検査とinventory_contractを再利用する。
新しいSCM/native builder、旧231のprofileコピー、actual captureの仮生成は行わない。
GPU source/署名品はmain管理、USB1121も未結合のまま。現段階はheader生成を持たず、
`build_allowed/native_patch_allowed/device_operations_allowed=false`の候補行だけを出力する。

```sh
python3 -B tools/recovery/peripheral-signed-updates.py --workspace "$ROOT" \
  --out port/xnu/out/peripheral233-candidate-preparation
```

`peripheral-signed-updates-accepted.json`は実public cold1033署名manifest/main reviewと
実keypad14署名manifest/main review/全trioを固定する。増分signed1033やunsigned coldを
同versionという理由で使わない。compiled/signing/metadata/observation sourceは分けたまま。
keypadの原manifestのunsignedというschema名やdeployable=falseを改名しない。
これは署名候補の準備受理であり、実配備許可/機能成功ではない。

## mainから必要な実入力

`peripheral233-baseline.pending.json`のnullを実値へ置き換えたmapをroot内へ保存する。
source内のpending原本は未確定状態のまま保持する。

- fresh: actual233 manifestのroot相対path/SHA。Recovery UUIDは同manifestの実値。
- native: 同fresh SHA・同Recoveryへ結合した1回読取りreceiptのpath/SHA。
  capture_number233、new MT8167CombinedFollowup233 absent、全old_services fingerprint、
  全captured_named_values、SYSTEM trio原本不変、full_service_capture_complete=true、
  registry_write_requested=false、reader cleanup各exit0が必要。
- additional_service_names: MT8167CombinedFollowup231を含む既service集合。省略しない。
- capture_contract: fresh/native SHAとRecovery、service_namesとnamed_value_ids全件を固定する。
  schemaはSIGNED_PERIPHERAL_UPDATES_FRESH233_CAPTURE_CONTRACT_V1。
  これは新hardware観測ではなくactual receiptの全件集合を明示する入力契約。
- template_policy: 最後に採用された実policy path/SHA。旧service/値の集合を省略しない。

```sh
python3 -B tools/recovery/peripheral-signed-updates.py --workspace "$ROOT" \
  --baseline-map <実input-map> --out <未作成のroot相対出力>
```

このbaseline検証でもfinal profile/headerは生成しない。nativeの
full_service_preservation_equalがnullであることをtrueへ置換しない。
既shared200.inventory_contractで、実receiptとcontractの全service/値集合の完全一致、
旧policy集合の包含、root/subtree/raw値schemaを検査し、その実値を次policyへ渡す。
既定のservice guard・動的CP以外のfingerprintを緩めない。

## mainのGPU profileへの接続

既shared200 TIMING.adopt内でwifi/keypadの場合だけ本moduleのadopt(root,role,baseline)を
使い、GPUはmainの専用adapterへ委ねる。他11roleは同captureの実保持package行のまま。
shared200本体はadoptした全roleのtrioと原manifestをinput_pinsへ追加するよう共通化した。
finish時は本moduleのinput_refs(root)、accepted lock本体、module本体、CP headerを
policyのinput_pinsへ必ず結合する。GPU/USBを勝手にinstall_targetsへ追加しない。
wifi+keypadだけのmaskは136。最終install mask/restart回数はGPU/実PnP契約確定後の判断。
全13roleの実baseline/package行とprimary/supplement全file hash/size検査は既build_policyで維持する。

最終化にはさらに同actualの補足assets/mutable manifest、全13role actual_package_rows、
保持boot出自、post値・raw filter root/Owners/DisplayName証拠、client/diagnostic refsが必要。
これらは既shared200/GPU adapterへ渡す。ここで未確定GPUを旧379/231へ仮固定しない。
SCM/native生成はGPU profile/schema/whitelistへの正式接続後に既200 builderで一括し、
現段階で新profileがbuild入口を通るとは主張しない。runnerはreadonly作業に専有されており使用しない。

keypad14はB25..B31を書き得るため、最終headerは既A04_KEYPAD13_CHECKPOINT=1に加え
A04_KEYPAD14_CHECKPOINT=1を明示する。未指定時はB24までの旧guardを保持。
新stageだけにwriter由来kind/aux/knownmask/press ID/byte数/完了復帰maskを限定し、
80B shape/magic/version/epoch・stale区別・子key拒否・前後epoch再確認を保持する。
全service/rootの免除は追加しない。将来postの同load readerも14のschemaを受理するsourceが必要で、
現Hegelのactual233読取り中sourceは変更しない。このheader拡張のWindows compile/実行はまだ未検証。

USBは5c39277d C/19e再結合sourceと実component packageの一致、起動/終了/flushの同run証明、
新fresh/final GPUとの正式再結合が必要。旧GPU369専用pinを使わず、現prepへUSBを混ぜない。
実機操作、署名、driver/SDK buildは行わない。

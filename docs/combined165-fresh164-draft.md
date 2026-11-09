# 165：fresh164待ちの非arm模板

`tools/recovery/prepare-combined165-draft.py` はrootの固定163contractとBT2016/provider1025 pinsを照合し、contract-DRAFT.jsonだけを新規生成する。既163/347、hive、媒体、署名serviceは変更しない。SDK/compile/署名/実機操作0。

```sh
python3 -B tools/recovery/prepare-combined165-draft.py --workspace ROOT --out ROOT/port/xnu/out/windows-rt-combined165-fresh164-draft
python3 -B tools/recovery/prepare-combined165-draft.py --workspace ROOT --check-media ROOT/port/xnu/out/windows-rt-combined165-fresh164-draft/contract-DRAFT.json
```

後者は必ずexit2で媒体生成を拒否する。draftは正式builder入力ではない。source/Recovery/停止service162の5値/actualGPU結果は未到着のためnull。raw5に推測Start4や既hashを代入せず、旧40namedはreference-only、最終45namedはnullとする。fresh162を164と偽装しない。

既BT2016+provider1025対のみDIF（role wifi idx3、bt idx12、mask0x1008）、予定restart1、既ROOTのみ更新・root登録0、BTstatus idx0 --statusを30s上限1回。GPUはfresh164 actual保持・再試験0、他client0。install900s、boot180sとphase1最初PB286起算180sを別枠とする模板。GPU348はmainが明示通知し正式seal完成した場合だけscope更新する。

正式化にはmain確認fresh164 manifest/original41/nativepost/Recovery、実service162のtyped raw5、45named・fullservice・全actual tuple、最新mutablelog baselineが必要。readerは既read-combined162-postと新BT2016 helperを基点に同clone/readonly mount guard・失敗証拠回収を保持。新42555796562の既cacheを使い、正式scope確定後のservice1TUだけ増分生成する。新hostに旧C:workspaceが存在すると仮定しない。

BT候補のD0Return0は有限software admission待機開始を含む。HCI成功/Code10解消とは扱わない。次main batchでstage10/18/11/8/19/20と実radioを照合する。

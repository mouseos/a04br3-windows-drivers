# copy334結合時の増分build担当と手順

Menciusはtools/ciのsource recipe・dependency planner・incremental archive executor・最終package/sign統合を担当する。Huygensはcopy/wire/IRQ/retireを担当し、runtime共有fileをこの作業で変更しない。統合branchは`integrate/copy334-wddm-aperture`一本とし、新deltaのbase/headを受領後前方mergeする。

## 実装済みのbuild経路

`plan-ge8300-incremental-runtime.py`は実Clang `-M`で各TUの実includeを取得する。Git基点blobと比較し、共有headerが変わればすべての実callerを再compile対象へ。source/compile flags/新object/外部header出自の変化も再compileへ。未pinの実SDK header、変化したcompiler、dirty tracked sourceは拒否する。TUを手書きリストだけで省かない。

`build-ge8300-runtime-plan.py`はplan hash、baseline archive/receipt hash、compiler/ar hash、全dependency hashを再検証してから実ARMNT compileする。非変更objectは元archiveから抽出しhash一致を要求。再利用のSDK/header出自が無ければ拒否。削除memberは明示し、archive全member順を照合する。signed SYSを生成したと偽らず、native/KMD等のcomponent単位archiveとmanifestだけを生成する。

この経路はコード生成flag・外部SDKを推測しない。`--current-units`/`--prior-units`は全componentのsource/object/argumentsの実recipe、`${SOURCE_ROOT}`以外の外部pathsは既存toolchainの固定値を渡す。`--external-pins`は実include全fileのpath/hash。baseline receiptは対応source_commit/clang_sha256/archive_sha256/member順hash/unit flagsを持ち、再利用用unitsには実外部dependency出自が必要。旧SDK出自が無ければ該当TUを再compileする。

```powershell
python tools/ci/plan-ge8300-incremental-runtime.py `
  --source-root . --base-commit $baseCommit `
  --current-units $currentUnits --prior-units $priorUnits `
  --clang $clang --clang-sha256 $clangHash `
  --external-pins $toolchainPins --out $newPlan

python tools/ci/build-ge8300-runtime-plan.py `
  --source-root . --plan $newPlan --plan-sha256 $planHash `
  --baseline-archive $priorArchive --baseline-receipt $priorReceipt `
  --baseline-receipt-sha256 $priorReceiptHash --clang $clang `
  --llvm-ar $llvmAr --llvm-ar-sha256 $llvmArHash --out $newBuild
```

全sourceの通常入口は既存`build-ge8300-from-source.ps1 -EnableCandidate`。native67/compiler375 source gateと単一checkout検証済みで、PCO/SDK全再buildは今回の必要工程ではない。最終Huygens deltaで新TU/ABIを追加する場合はsource map/originsと通常入口も同時更新する。

## 成功範囲と残件

planner: 実Clang17caller依存解析、共有/専用header、flags、新TU/削除member、SDK/compiler/dirty source拒否の12case。executor: 実thumbv7 Windows ARM COFFで2TU再compile+1byte保持、無変更3object再利用、archive破損/plan後HEAD変更拒否の4case。GPU実sourceの新delta実buildや実機試験をこのfixture成功へ読み替えない。

GPU署名済み正本は51cbe2d4の12TU成果。最新wire/WDDM/IRQ/retire未受領のため、全runtime結合packageは未完成。delta受領後はdependency plan→affected TU compile→real SYS/DLL link→署名→INF/CAT更新・全member trust→baseline provenance/FW/client保持pin→単一checkout/source recipe/pinsを完成させる。公開最終packageにintegration-only archiveの状態を成功として書かない。実機ownerはmain、媒体fresh基点はmainから受領する。

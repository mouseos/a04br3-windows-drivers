# GPU371 output: run213 / fresh212 / post214

source54984933から分岐した`work/gpu371-output-controller`。既200/369 generatorと共通controller/Observationを再利用する既知variantで、GPU実装・Windows build/native PS・collectorは変更しない。

- 固定profile: `combined-features-fresh212-gpu371-output-v1`
- 固定service: `MT8167CombinedFollowup212`
- INSTALL=`['render']`、mask1024、予定restart1、Present363/helper198を同boot各1回。
- 実baseline render369/ce0c8aeb...だけをrender371へ更新する。display369・WiFi1029・UMD/FW・他rolesは実fresh/nativeの選択と実byteを保持。
- boot98deは209実write provenanceから保持し、今回boot/GPT書込み0。候補を保持bootの新書込み成功としない。
- terminalは既same-boot/run/PID、SCM-stop/flush gateでWindowsを保持する。capture214はmainの明示collect-onlyだけ。候補install/clientの再実行とcollect二度目を既latch/専用outputで拒否する。

## 閉じた実371入力

- package: `port/xnu/out/windows-rt-vidpn370-relink/signed371/package371`
- signed-manifest SHA256: `649612e1b5facd4b0992fea6b2c7c2a37cd2ea47d0570bcbf97b28c871b8655b`
- render SYS: `202f0f3fd7d4325df20dd8570d88299f008fae421bff16cccf99589282a19ded`
- 実main review: `port/xnu/out/windows-rt-vidpn370-relink/main-signed371-review.json`
- review SHA256: `140608b346efcdaba6be0428610f3f8628a647cf26f76bd7fb8ac1f11977e1bc`
- runtime b63827d7 / BDD compile91f14b4c / relink-sign recipe9d941f98を別々に保持。
- link1/compile0、新5WDKlib lock922294e3...を今回link入力として保持。historical他4lib equality=falseを成功へ変更しない。

未知のmanifest SHA、別recipe/runtime/version/packageは不可。base.packageへ追加したnative manifest解釈も上記実SHA/recipe/runtimeのrenderだけで、任意package対応ではない。

## mainの実fresh212確定後のmap

`prepare-gpu371-output-inputs.py`のmap schemaは`GPU371_OUTPUT_FINALIZE_INPUTS_V1`。
`capture_number=212`、上記profile、`hardware_owner=main`、`install_targets=['render']`を指定する。
次の各refは既入口と同じ`{path,sha256}`。未取得のref/null/hash0は通らず、出力生成前に拒否する。

- `fresh`: 実`port/xnu/out/private-windows-rt-prebackup-run212/manifest.json`
- `native`: 同じfresh/hash/bootに結合した実read-only inventory。全oldServices/named値/nodes、POST0、ControlSet001、new service不在を既validatorで検証。
- `capture_contract`: schema=`GPU369_FRESH212_CAPTURE_CONTRACT_V1`（実producer形式をそのまま使用し371へ改名しない）。実native/file/service/named値集合をmainが固定する。旧208/210の改名流用不可。
- `template_policy`: 既実369 policyの参照。旧fresh/sourceを新実測と呼び替えず、新native集合を照合するために使う。
- `actual_package_rows`: 全13rolesの実選択package行。render369/display369/WiFi1029を含む。候補371をactual baseline行へ置かない。
- `GPU371_manifest`と`GPU371_handoff`: 上記実manifestと実main review。
- `supplement`: 必要時の同じRecovery bootの実assets補足。`mutable_baseline`は実Present assets/既GPUログbaseline。
- `globaldiag_manifest`/`globaldiag_import_proof`: 既helper198の固定署名/実RT8.1 import proofを再使用。
- `boot_witness`: 下記実保持chain。
- `main_execution_approved`: mainの承認が実際に確定するまではfalseのまま。このsourceはtrueを生成しない。

```sh
python3 -B "$repo/tools/recovery/prepare-gpu371-output-inputs.py" \
  --finalize --workspace "$root" --map "$Actual212Map" --out "$New371HeaderOutput"
```

このコマンドはsource header/policy生成のみ。新fresh、承認、native hive、媒体、実機成功を作らない。今回未取得fresh212でのfinalize/Windows build/配備は実行していない。

## boot witness: 実209 writeからの保持

schema=`GPU369_RETAINED_BOOT98DE_CHAIN_V1`（取得済み実schemaを使用）、`source_manifest_sha256`/`recovery_boot_id`は実fresh212、`read_only=true`、`physical_read_this_capture=false`、boot_sha256=98de46d3...を明示する。
`last_write_seal`は実GPU369_RUN209_MAIN_SEAL、`last_write_result`は同seal SHA/transactionに結合した実209 result（boot_written=true、matched_boot_applied_before_normal=true、GPT0）。
`host_artifact`/`firmware_composition_manifest`を同sealの実pinへ結合し、FDc731f8.../a82e2713/DTM0を照合する。
`run209_collection_result`のboot_written=trueは累積run209状態として検証し、新たなwriteとは扱わない。実210/212/assetsの`readonly_capture_refs`をhash結合する。`no_intervening_boot_or_gpt_write_reported=true`と`no_additional_write_report`はmainの実報告であり、連続独立監視や物理boot readの証明ではない。そのscopeをpolicy receiptに残す。実fresh212未取得の段階でこのwitnessを合成しない。

## main sealとcontroller

既seal構造の値をrun213へ固定する。schema=`GPU371_RUN213_MAIN_SEAL_V1`、transaction=`combined-run213`、service/data directoryは上記212。
`controller_path`は新`run-gpu371-fresh212.py`、`controller_engine`はこのbranchの共通`run-gpu368-fresh200.py`。
追加の`generator_path`はこのbranchの`prepare-gpu371-output-inputs.py`。これらとcollector helper、実policy/input refsをpinned_filesへ含める。
`matched_boot`/`matched_boot_manifest`/`matched_boot_sha256`は保持witnessの同host実体、`boot_write_required=false`。boot_written/GPT/hardware_success_claimedは実行前false。
既Media/署名service/nativeはmain担当。新profileをWindows recipeへ受け入れる変更が必要ならmain側でこの固定variantだけを追加する。旧549/vidpn-output worktreeへ本作業から書き込まない。

```sh
python3 -B "$repo/tools/recovery/run-gpu371-fresh212.py" \
  --workspace "$root" --seal "$Main213Seal" --seal-sha256 "$Main213SealSHA" \
  --output "$root/port/gsi/logs/ios-feasibility/windows-rt-combined-run213-runtime" --validate-only
```

実行はmainだけが`--validate-only`を外す。terminal後のWindowsを保持し、mainが回収を選ぶ場合のみ同じ引数へ`--collect-only`を追加する。`--collect-only --validate-only`は既held terminalのread-only検証。二度目のcollectや候補再実行は不可。
出力captureは`port/xnu/out/private-windows-rt-prebackup-run214`、diagnosticsは`port/xnu/out/private-windows-rt-gpu371-run213-diagnostics`、latchは`gpu371-run213-executions/combined-run213.json`に分離。

## mainの原本保持copyも受理

mainのprivate-map371.jsonが指定する`capture-contract.gpu371.json`（GPU371_FRESH212_CAPTURE_CONTRACT_V1）と`retained-boot-witness.gpu371.json`（GPU371_RETAINED_BOOT209_V1）も受理する。`schema_adaptation_of`の実path/hashをpinし、schema/明示alias/原本参照以外の全事実・集合が原本と一致することを要求する。boot aliasはhost_artifact/firmware_composition_manifestと完全一致させる。main-retention-report212はreported_by=main、physical_read_performed=false、fresh212 hash結合を保持し、そのcapture/supplement/return-normal参照もpinする。原本/copyのいずれでも事実や実測成功へ変換しない。

## 既SCM/native/media CLIの371明示入口

旧`-Gpu369Timing`/`--gpu369-timing-profile`は従来どおり。371は次の専用switchだけを使用する（両variant同時指定不可）。

```powershell
& "$src\tools\ci\build-combined200-service.ps1" -Gpu371Output -CaptureNumber 212 -Mode Service `
  -Workspace $Workspace -PackageMap $Actual212Map -OutputDirectory "$src\out\service212" `
  -KitHeaderRoot $SavedKitHeaders -LibraryRoot $SavedArmLibs -SigntoolPath $SavedSignTool
```

生成済みpolicyを渡す既`prepare-offline-system-service.ps1`はservice212/profile371/render-only/bootfalse/fresh4e34/47services277values160filesを要求する。新しいnative実装は追加せず、既clone-only/privrestore/全旧Services保存・named値確認を共用する。

```sh
python3 -B "$repo/tools/winpe/build-private-peripheral-batch-media.py"   --gpu371-output-profile --enable-peripheral-batch   --workspace "$root" --map "$Actual212Map"   --service-package "$SignedService212" --prepared-system "$PreparedSYSTEM212"   --prepared-result "$PreparedResult212" --prepared-sha256 "$PreparedSYSTEM212SHA"   --comparator "$SavedComparator" --comparator-manifest-sha256 "$SavedComparatorManifestSHA"   --base "$SavedBaseMedia" --tools "$SavedWinPETools" --source-commit "$FixedSourceCommit" --out "$New371Media"
```

service212のnative-result admission、runtime-token検証、署名EXE/CAT所属検証、collectorのGPU371_RUN213_MAIN_SEAL_V1対応まで既経路へ結合する。role/package validatorは実6496/native371/9d941/b638/91f14だけを受理し、INF71はsigned369 INFから署名recipeがDriverVer変更した出自として扱う（Git上の旧INF54をINF71と偽らない）。
これらのコマンド実行・最終profile/header/native/media出力はmain担当。本source作業では実行しない。

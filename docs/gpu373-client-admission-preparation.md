# run216 / fresh215 / post217

`prepare-gpu373-client-inputs.py --workspace ROOT --map out/windows-rt-gpu373-finalize215/private-map373.json --out NEW --finalize`
で、既200 generatorとC emitterを再利用する。実fresh215/native215/同Recovery補足215、371の署名manifest/review、全13実package行、既boot98deの保持chain、正式署名client pairを必須とする。mapは `GPU373_CLIENT_UPDATE_ADMISSION_V1`、profileは `combined-features-fresh215-gpu373-clients-v1`、serviceは `MT8167CombinedFollowup215`。

既371mapのfresh/native/capture_contract/template_policy/actual_package_rows/GPU371_manifest/GPU371_handoff/boot_witness/mutable_baseline/globaldiag_manifest/globaldiag_import_proofを使い、実215へ結合する。`baseline_policy`は実371/369/1029を持つ既policyのpath/SHA。`client_pair.launcher/evaluator`はそれぞれ `file`（署名EXE）、`manifest`（元receipt.json）、`import_proof` のpath/SHA参照。flatな署名manifestへの変換は不要。evaluator source22030aed、launcher sourcef8638418。正式pair directoryの2本だけを使い、unsigned.exeは入力しない。

`wifi_update`は正式1030のsigned-manifest.jsonのpath/SHA。行のsource/version/dateは実manifestから導出し、既INF/CAT/member verifierへ渡す。指定時は `install_targets:["wifi"]`、DIF mask8、restart1。省略時は `install_targets:[]`、mask0、diagnostic-only、restart0。kernel371/transport369、UEFI/boot/GPT/BT/親の更新は禁止。372は採用しない。SCMは共通1TUのまま。

capture_contractは `GPU373_FRESH215_CAPTURE_CONTRACT_V1`、capture_number215、fresh_manifest_sha256/native_receipt_sha256/recovery_boot_id/file_count/service_names/named_value_ids。集合と件数は実215から導出する。boot_witnessは `GPU373_RETAINED_BOOT215_V1`。previous_witness（実212の保持証拠）、run213_seal/result/collection、capture214をpinし、実215のsource_manifest_sha256/recovery_boot_id、既host_artifact/firmware_composition_manifestを結合する。physical_read_this_capture=false、continuous_independent_monitoring_performed=falseとし、no_later_boot_or_gpt_write_reported_by_main=trueはmainの既保持指示のscopeに限る。

Windows入口は `tools/ci/build-combined200-service.ps1 -Gpu373Client -CaptureNumber 215 -Mode Service`。既Workspace/SDK/libs/証明書を使い、source/outputはcheckout-local、新出力のみ。nativeは既 `prepare-offline-system-service.ps1` にこの生成preservation-planを渡す。新215が不在であることを要求し、旧48svc/284valuesを同じ集合で保持する。既読取りnative215の再実行は不要。native patchは実SYSTEMのコピーだけを使う。

media入口は `build-private-peripheral-batch-media.py --gpu373-client-profile`、既定出力はmain側 `out/private-windows-rt-combined-followup215-media`。旧clientをbaseline-witnessで照合してから正式pairのみ上書きし、新hashをSCM保持guardへ渡す。runtime tokenは既共通generatorと同じgpu_client_count=1へのadaptationを維持し、canonical policy hashとは区別する。

controllerは `run-gpu373-fresh215.py`。main sealは `GPU373_RUN216_MAIN_SEAL_V1`、private_map/generator_path/controller_engine/collector_present_helperをpinし、planned_windows_rebootsとwifi_activationをpolicyと一致させる。terminal成功・失敗とも通常Windows保持。panic/想定外restartは既復旧経路、回収は明示 `--collect-only`。同candidate再実行は禁止。実機操作はmainのみ。

host testは `tests/gpu373_pipeline_test.py` と既controller hold/371/369テスト。成果物生成、署名成功、実機成功は別で、sourceだけではruntime成功を主張しない。

## client374の薄いvariant（fresh218 → run219 → post220）

`prepare-gpu374-client-inputs.py`は同じgenerator／admission／C emitterを利用する。profileは `combined-features-fresh218-gpu374-clients-v1`、serviceは `MT8167CombinedFollowup218`。別の全profile・driver recipeは持たない。

map schemaは `GPU374_CLIENT_UPDATE_ADMISSION_V1`、capture_number218。前述の実入力refと `launcher_source_commit`（実署名receiptと一致する正式40桁commit）が必要。evaluator runtimeはmain承認 `5eb3ab11d9e63a5e07a752f84766dc4b1eb292ad` に限定する。`wifi_update`は指定不可、install_targetsは空、mask0／diagnostic-only／restart0。kernel371／transport369／WiFi1030の全packageをbaseline fingerprintと同一に保つ。372は採用しない。

capture contractは `GPU374_FRESH218_CAPTURE_CONTRACT_V1`。boot witnessは `GPU374_RETAINED_BOOT218_V1`、previous_witnessに実215の `GPU373_RETAINED_BOOT215_V1` を指定し、実run216のseal/result/collectionと実capture217を結合する（keys: run216_seal/run216_result/run216_collection/capture217）。従来の209→213保持chainを再利用し、今回もphysical bootreadや独立連続監視を主張しない。

入口は `build-combined200-service.ps1 -Gpu374Client -CaptureNumber 218 -Mode Service`、mediaの `--gpu374-client-profile`、controllerの `run-gpu374-fresh218.py`。これらは373共通実装へdispatchするalias／薄いfacade。main seal schemaは `GPU374_RUN219_MAIN_SEAL_V1`、generator_pathは新374 facadeをpinし、private_mapもpinする。SCM/nativeの登録対象は新218のみ。通常terminal成功・失敗はholdし、予定restartは0。公開SetDisplayConfigによるpath activation自体はclient側で行い、controllerが独自に設定変更することはない。

未取得fresh218・未確定署名hashをこの文書やsourceから生成しない。実入力が揃うまでfinalize／build／native patchを実行しない。readonly inventoryは実capture通知後、既 `read-fresh200-native-inventory.ps1 -TimingCaptureNumber 218 -IncludeWifiCheckpoint -IncludeGraphicsTopologyCache -IncludeDisplayDeviceBindings` の1loadを利用できる。1030 wire format対応readerを含む。


## client375 / fresh221 / run222 / post223

基点ea691478。375は共有variantの221→375で、runtime sourceは
`3247c8923871630e14313aa00c9ccc4085ca03b6`だけを受理する。
`prepare-gpu375-client-inputs.py`は共有generatorの薄い入口。
map schemaは`GPU375_CLIENT_UPDATE_ADMISSION_V1`、capture221、
profile `combined-features-fresh221-gpu375-clients-v1`、新service
`MT8167CombinedFollowup221`。install_targets=[] / diagnostic_only=true /
planned_reboots=0 / mask0。render371・transport369・WiFi1030・boot98deを保持。

fresh/native/同Recovery補足/署名pair/imports/正式launcher commitが揃うまでfinalize不可。
launcherはmapの正式commitとreceipt runtime_source_commit、evaluator source/hashを結合する。
未知のhashや新captureは作らず、既recipeで実sourceをcompile/signしたreceiptだけを使う。
boot witnessは`GPU375_RETAINED_BOOT221_V1`、実218 witnessをprevious_witnessとして、
実run219 seal/result/collection/capture220を結合する。物理boot再read済とは表示しない。

公開read-only inventoryは既`read-fresh200-native-inventory.ps1 -TimingCaptureNumber 221`。
WorkspaceRoot/Manifest/ExpectedManifestSha256/SourceDirectory/OutputDirectory/PolicyPath/
ExpectedPolicyFileSha256を実値で指定。新221不在・全旧service/値を実測し、過去の件数を流用しない。
private cloneの一回loadのみ。`-IncludeWifiCheckpoint -IncludeGraphicsTopologyCache`
を付ける場合も同loadで読む。mainの実fresh221完成通知前には実行しない。

実入力到着後の入口:
- `prepare-gpu375-client-inputs.py --workspace ROOT --map MAP --out NEW --finalize`
- `build-combined200-service.ps1 -Gpu375Client -CaptureNumber 221 -Mode Service`（共通SCM1TU）
- `prepare-offline-system-service.ps1`へ生成planと実SYSTEM trio、新221serviceだけを渡す。
- mediaは`--gpu375-client-profile`、controllerは`run-gpu375-fresh221.py`。
- sealは`GPU375_RUN222_MAIN_SEAL_V1`。terminal後Windows保持、回収はcollect-only、同candidate再実行禁止。

### 任意OS runtime論理回収（端末操作はmainだけ）

既`collect-combined200.sh SEAL RESULT NEW_OUT --gpu-runtime-assets`は既RecoveryのRO mount内で
`fixed/Windows/System32/{user32.dll,win32k.sys}`だけを追加回収する。既定は従来どおり。
controller sealで`gpu_runtime_assets=true`を指定する場合、
`collector_gpu_runtime_helper`を`collect-gpu-runtime-assets.py`へ向けpinned_filesにSHAを含める。

mainが同Recoveryのfresh221補足だけ先に回収する場合は、mainが所有する既
`/tmp/rt125-diag`のRO mount内で次の公開helperを使用できる（helper自身はmount/umountしない）:

```
python3 -B tools/recovery/collect-gpu-runtime-assets.py --workspace ROOT --serial SERIAL --capture-manifest ACTUAL_MANIFEST --out NEW_ROOT_LOCAL_OUT
```

上限は各16MiB、SYSTEM原本hash/Recovery boot ID/partitionを前後照合し、元manifest byte不変を検査。
別receipt `gpu-runtime-assets.json`へhash/size/missing/errors/completeを記録する。
欠落を成功にせず、capture本体のfiles/generation/原本は変更しない。論理RO copyであり物理readbackではない。
署名・実機・API機能成功はこのsource/host検証とは別。

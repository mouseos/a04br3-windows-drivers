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

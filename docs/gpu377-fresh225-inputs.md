# renderer377だけを更新するrun226

実fresh225/new service225/run226/post227。旧post223-resume224は履歴だけで、次writeのSYSTEM/native基点には使わない。

- generator: `tools/recovery/prepare-gpu377-fresh225-inputs.py --workspace ROOT --map ACTUAL_MAP --out NEW_OUTPUT --finalize`
- SCM: 既`tools/ci/build-combined200-service.ps1 -CaptureNumber 225 -Gpu377Output -Mode Service`。
- media: 既`tools/winpe/build-private-peripheral-batch-media.py --gpu377-output-profile`。
- controller: `tools/recovery/run-gpu377-fresh225.py`。seal schema `GPU377_RUN226_MAIN_SEAL_V1`。

SCM/native/media/controllerはrender377だけDIF1024/restart1、boot98de/GPT0、Present client375/helper198各1回、terminal後Windows保持。
collect-onlyは既engineのhold検査を再用し、candidate再書込み・診断再実行をしない。
新frame/driver/SDK/UMD/FW/client buildは追加しない。

入力mapのschemaは`GPU377_FRESH225_FINALIZE_INPUTS_V1`、profileは`combined-features-fresh225-gpu377-output-client375-v1`。
`capture_number=225`、`hardware_owner=main`、`install_targets=[render]`、`main_execution_approved=true`はmainの実承認後だけ。
全refはroot相対pathと実SHA256。

必須ref：fresh、supplement、native、capture_contract、template_policy、actual_package_rows、GPU377_manifest、GPU377_handoff、boot_witness、mutable_baseline、globaldiag_manifest、globaldiag_import_proof。

- freshは実225 MF9aa07e69、42file。supplementは実225 assets MFf148fa18、127file。mutableは同補足9e2052a7。
- nativeはSingerの実225同load receiptのみ。new service225不在、全旧service/値集合、trio、選択376/372/1031/1.8を検査する。未着ならfinalize不可。
- capture_contractは既200 generatorのshape、schemaだけ`GPU377_FRESH225_CAPTURE_CONTRACT_V1`。file_countは実fresh+補足の重複排除集合、service_names/named_value_idsは実nativeの集合。旧件数を埋めない。
- template_policyは既正式375combined policy。形式・client375署名pairの履歴参照だけ。新基点の値・trio・選択・logsは全て実225から再検証する。
- actual_package_rowsは実225選択に対応する13role。renderのbaselineは376のまま残しcandidate377と区別。UMD/FW/transport372/WiFi1031/電池1.8は保持する。
- GPU377_manifestは実767c1877、SYSb4a2a17f/source95600846。GPU377_handoffは実main-independent-review.jsonへのSHA ref。
- boot_witnessは`GPU377_RETAINED_BOOT225_V1`。実225のsource_manifest_sha256/recovery_boot_id/capture225 ref、previous_witness（既221までのchain）、run224_seal/run224_result、boot_sha256/host_artifact/firmware_composition_manifest、physical_read_this_capture=false、no_later_boot_or_gpt_write_reported_by_main=true、continuous_independent_monitoring_performed=false。既209write証拠へ接続し、main保持申告を独立監視へ昇格しない。

SCMの377分岐だけ`A04_DEVELOPMENT_DISPLAY_ON=1`をclangへ明示し、main担当C/headerをそのままcompileする。
receiptのcompile_closureへC/header SHAとmacroを保存し、mediaが固定source/同headerを照合する。
power helperはoptional失敗/rollbackの記録後もGPU診断を継続するmain実装。buttons修正やtarget成功とは主張しない。

source固定と実native取得より前にSCMsign/hivepatch/媒体生成/deviceへ進まない。このsource作業では実行していない。

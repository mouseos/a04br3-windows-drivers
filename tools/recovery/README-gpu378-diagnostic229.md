# 実229保持tupleの診断専用followup

基点b47e694a。378/1.3のDIFはrun228で両方成功したが、後段のpreserved root比較が1306で停止し、予定restart0・client未実行だった。実229はrender378/keypad1.3選択とSYS、UMDc901を確認済み。差分はMt6392Keys rootのDisplayName（indirectから同名plain）とOwnersへのsource INF追加だけ。旧312named値は一致。旧52rootガードを一般化して緩めず、実229の現在rootを新基点にする。

新service MT8167CombinedFollowup229、fresh229 → run230 → post231。install_targets空、mask0、diagnostic_only=true、driver_updates=false、planned_reboots0。現在の全13role/SYS/UMD/FW/client375/transport372/WiFi1031/battery1.8/boot98de/GPTを保持する。raw native229では旧service227 Start=4を確認し、次bootで旧候補DIFを再実行しない。

既200/378 generator・SCM compiler・media・once-only engineを共有し、class Diagnostic229で実capture/retained modeだけを限定する。A04_DIAGNOSTIC_ONLYの既コードはinstall pathを通らずtransaction(1)へ進む。次normal bootでPresent375/helper各1回、既CP guard/HID bindings/Powerscheme helperを実行し、terminal後Windows保持。collectは別入口。OS/device/contextや画面成功は事前に保証しない。CP rawはpost231の同load reader162c2ad3系で回収する（CP欠落を成功へ補完しない）。

## 入力と入口

`prepare-gpu378-diagnostic229-inputs.py --workspace ROOT --map MAP --out NEW_OUT --finalize`

map schema `GPU378_DIAGNOSTIC_FRESH229_FINALIZE_INPUTS_V1`、profile `combined-features-fresh229-gpu378-keypad13-diagnostic-only-v1`、capture_number229、install_targets空、hardware_owner main、main_execution_approvedは既実行許可による明示入力。

ref項目は既378 mapと同じ（fresh/supplement/native/capture_contract/template_policy/retained_client_policy/actual_package_rows/GPU378_manifest/GPU378_handoff/keypad13_manifest/keypad13_handoff/boot_witness/mutable_baseline/globaldiag_manifest/globaldiag_import_proof）。template_policyは実署名service227 policy。actual_package_rowsはその保持tupleを実229の選択とbyteに照合する。nativeは最初の229 single-load receipt0d23a6e3固定。Capture contractはGPU378_DIAGNOSTIC_FRESH229_CAPTURE_CONTRACT_V1、boot witnessはGPU378_DIAG_RETAINED_BOOT229_V1。実229 MFfb899d17/Recovery53000fc0、assets e3a73d3c/mutable0e0821ecだけを受理する。

SCM入口は既`build-combined200-service.ps1 -CaptureNumber 229 -Gpu378Diagnostic -Mode Service ...`。nativeは既prepare-offline-system-service.ps1、新service229だけ。mediaは既`build-private-peripheral-batch-media.py --gpu378-diagnostic-profile ...`。controllerは`run-gpu378-fresh229-diagnostic.py`、seal `GPU378_DIAGNOSTIC_RUN230_MAIN_SEAL_V1`。既更新378入口と旧controller契約は維持し、任意profileやpackageの許可は追加しない。

Power helperは既承認コードの診断前適用＋診断後readonly readback、追加fallbackなし。新driver build/sign/再DIF、元227/229の再inventoryは不要。必要な新buildはSCM1TUだけで、SDKは保持cacheを使う。

# 周辺一括更新 source と発火条件

このprofileはGPU診断媒体と分離する。USB1009、MMA1.10、AJTD4R eMMC1.9、親1101を既存4nodeへ通常OSのDIFで延期installする。RTC5（DriverVerは0.0.1.0）とthermal17は先にstore stageするだけで、親更新後の起動時bindを待つ。既存Engine/renderの再試験、旧driverの再install、校正変更は含まない。

4076878d時点はsourceのみ。Cのcompile/link、専用serviceの署名、private hive replay、媒体生成、実機検証は未実施。既存候補の署名済みSYS/CATとは区別する。main指定のfresh114以降が未結合のdraftからはheader/build/arm/mediaを生成できない。

## main指定captureの結合

番号の自動探索や「最新らしい」ディレクトリの選択はしない。mainが同じRecovery bootで無変更のcaptureを指定し、次のJSONをroot内に作成する。下記の大文字項目は説明用であり、実値がないと拒否される。

```json
{
  "schema": 1,
  "profile": "peripheral-usb1009-mma110-emmc19-parent1101-v1",
  "capture_number": 114,
  "manifest_sha256": "MAIN_SELECTED_MANIFEST_SHA256",
  "recovery_boot_id": "MAIN_SELECTED_RECOVERY_UUID",
  "gpu_policy_sha256": "MAIN_SELECTED_GPU_POLICY_SHA256",
  "unchanged_since_capture": true
}
```

114より後でも明示した番号をそのまま使う。間にWindows起動やWindows変更があれば選択を破棄し、新captureを結合する。番号だけの変更では、manifest、native snapshot、trio、policy、boot IDの照合を通らない。

collectorにはSYSTEM/DRIVERS各trio、4更新対象SYS、MMAのEnum選択、旧サービスsnapshot用SYSTEM、保持対象Engine/render EXE、audio/WiFi/display/render/toprgu SYSを含める。capture内の全fileをhash/size照合するため、不足・差分はcaptureの再指定が必要。既搭載基点は親1026/USB1005/MMA1.8 bytes（選択INF版はfresh114実値0.0.1.0）/eMMC1.8の固定SYS hashのまま。ここに差があれば停止し、勝手にbaselineを更新しない。

`tools/recovery/read-peripheral-preservation.ps1` はWindows host上で新規private出力だけにtrioを複製し、native replayする。原trioはloadしない。必須引数は `-Workspace -Manifest -ExpectedManifestSha256 -CaptureNumber -ExpectedRecoveryBootId -ExpectedGpuPolicySha256 -SourceDirectory -OutputDirectory`。最新policyのLastDiagnostic generation/PolicyHashが同じreplayに存在することを確認し、4nodeの選択と旧5serviceのbounded subtree fingerprintを返す。Completeや機能成功の判定はしない。原trio hash不変・load/unload/privilege restore全0以外は失敗。

`prepare-peripheral-batch-draft.py --workspace ROOT --out NEW_ROOT_LOCAL_DIR` は既存候補からdraftを作る。既存draft v2も入力に使える。final mapの`fresh`は次の厳密集合とする。

```json
{
  "manifest": "port/xnu/out/private-windows-rt-prebackup-run114/manifest.json",
  "sha256": "MANIFEST_SHA256",
  "bindings": "ROOT_LOCAL_NATIVE_OUTPUT/peripheral-baseline.json",
  "bindings_sha256": "NATIVE_SNAPSHOT_SHA256",
  "main_selection": "ROOT_LOCAL_SELECTION.json",
  "main_selection_sha256": "SELECTION_SHA256"
}
```

同snapshotの`service_preservation`をmapへ写し、保持file pinsと親flags前後を埋める。旧5serviceはEngineProbe/RenderSingleGpu/RenderStage/DeviceInstall/RpcSs固定。native arm前後ではその全bounded subtreeを比較し、通常OSではroot値と保持fileを前後比較する。native snapshotの既知型・値64個・各64KiB・深さ6・key128個の上限を超えたら拒否する。

## 親とbootの対応

親1101と8range POST boot/FDは固定tuple。旧親を8rangeで起動する組合せも、1101を旧6rangeで起動する組合せも許可しない。

`boot_transition`には固定boot/FDの実byte pinsと、mainの別ファイルhandoff receipt（path/hash）を設定する。receiptの厳密集合は以下。

- `profile`: 上記profile
- `recovery_boot_id`: 選択captureと同じUUID
- `capture_manifest_sha256`, `gpu_policy_sha256`: 同capture/policy pins
- `mode`: `main-controlled-fastboot-handoff`
- `block_next_windows_boot`: true
- `current_parent_sys_sha256`: 固定1026 SYS pin
- `target_parent_sys_sha256`, `target_boot_sha256`, `target_fd_sha256`: 固定1101/8range tuple pins

計画Windows rebootでLK fastbootへ出た後、mainが対応bootへ切り替えてから次Windowsを開始する。旧GPU用の自動resumeは流用しない。serviceやmedia builderはFD/boot/GPTを書かない。計画Windows rebootは1回であり、SoC resetの回数を1と保証する契約ではない。

## final pin後のmain手順

1. `python3 -B tools/winpe/peripheral_batch_policy.py --workspace ROOT --map FINAL_MAP --out NEW_POLICY_DIR` で明示選択・全候補・保持条件を検査する。draftフラグをfalseへ変えるだけでは通らない。
2. clean sourceをWindows hostへ移し、root内の同じ相対配置で候補とcaptureを用意する。`tools/ci/build-peripheral-batch.ps1 -EnablePeripheralBatch -Workspace ROOT -PackageMap FINAL_MAP -OutputDirectory NEW_CHECKOUT_LOCAL_DIR -Sign -CertificateThumbprint BCBF6C456C6F0DD0818434763B189488FBF13570` をmainが実行する。専用1TUだけcompile/signし、全6候補のCAT membershipと署名identityも確認する。
3. `prepare-offline-system-service.ps1` に新cloneのtrio pins、`-ServiceName MT8167PeripheralBatch -PeripheralPreservationPlanPath PLAN -ExpectedPeripheralPreservationPlanSha256 PLAN_SHA` を渡す。既存7値登録契約・新service absent-only・旧service保持を通ったclean候補だけ使う。既存SingleGpu再arm profileの条件は変更していない。
4. `build-private-peripheral-batch-media.py --help` の必須引数へ専用署名package、同source commit、native候補/result pins、既存署名comparatorとguard付きbaseを渡す。明示`--enable-peripheral-batch`も必要。全stage hashと配置後EOF比較、現SYSTEM/DRIVERS各trio照合の後、最後に新capture由来SYSTEMをarmする。旧SOFTWARE/userfs/古いSYSTEMへのrollbackはない。

通常OSでは全候補を固定CAT/BCBF/hash検査→RTC/thermalを含む6候補stage→USB/MMA/eMMC/親の順でDIF→旧file/service保持確認→1回rebootを予定する。失敗後の自動再試行はない。次bootの選択版/date/service/class/SYS/OEM INF hash、DN_STARTED/problemを確認する。

RTC/thermal status取得はread-only IOCTLだけ。stale、not-ready、unsupported、cancel不明は失敗で記録し、成功stubへ変換しない。thermalはboot測定のため収集時にstaleになる場合がある。status読取り成功と温度・時計の実機機能成功は区別する。PB UARTはbounded queue受付のログで、機能成功証拠ではない。

## fresh114の補足保持capture

原manifest hashは変更しない。固定Render service EXE/UMD/FWの不足時だけ、同Recovery bootで原capture全fileの前後hashを確認したread-only補足manifestを`fresh.supplement`と`supplement_sha256`に結合できる。補足schemaは固定3file以外を拒否し、manifest hash/boot IDを原captureへ結合する。最終media stageでは保持file全件の実capture witnessを現Windows bytesと比較してからSYSTEMをarmする。

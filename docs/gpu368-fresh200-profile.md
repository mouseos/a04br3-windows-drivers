# GPU368・実fresh200専用の一括試験recipe

canonical2b695765を基点にした独立branch。既prepare196/facade196/service196入口はbyte不変のまま、fresh200専用prepare/facade/service/media入口を追加した。GPU368・PCO・SDK・USB driverの再buildは行わない。新policyを埋め込むSCMの1TUだけをmainの既Windows環境でcompile/link/BCBF署名する。こちらではWindows実行・媒体生成・実機操作をしていない。

fresh200は未取得。pending mapは入力仕様でありsnapshotではない。null参照のままfinalize/build/mediaを実行しない。過去196やUSBtrial199のsnapshotを200へ改名しない。実200取得後、mainが実manifest/native receipt/package行を固定してから実行する。

## 固定候補と保持

GPU368のsigned-manifest SHAはeef81706cf51711e375ddd7f6b46b6ef05fe7ed73c0ebd97dd64d484565fc32e。schema GPU368_SIGN_ONLY_FIXED198_V1、compile source a04e22489709db79d7c5a4355419bc7aaf95b057、署名recipe f25a594efc583455d5ac12f73c68de80c3e2fe7b。SYS/UMDは同manifestで結合し、全15leafを再照合する。GPU359/e072 runtime identityは継承値でありcompile sourceとは別。

install_targetsはrenderだけ、ROLES index10、DIF mask 1024、Windows restartは1回。旧transport357のSYS/INF/CATを同byte保持する。FWもfresh200の実byteと署名368のFWが一致する必要がある。parent1121・USB/HCDなどはactual_package_rowsと実node選択version/date/SYS SHAを照合し、保持する。旧parent1120を固定しない。13semantic targetを新USB HWIDへ勝手に差し替えない。HCDを含む追加driver fileと関連serviceは実capture・full service inventoryで保持する。

## 入力mapと実captureの条件

`tools/recovery/gpu368-fresh200-map.pending.json`のnull参照を、各 `{ "path": "root相対の実ファイル", "sha256": "実SHA64桁" }` へ置き換え、root内の新規private mapとして保存する。公開sourceのpending mapを書き換えない。

- fresh：`private-windows-rt-prebackup-run200/manifest.json`。read_only/unmounted/captured-unmounted/errors[]を要求。全原本fileのサイズ/SHAを検証する。
- supplement：必要な場合だけ、同Recovery boot・同元manifestに結合された実補足capture。既capture-peripheral-preservation.py相当の実回収のみ。重複fileはサイズ/SHA一致を要求し、原manifestを変更しない。UMD/FW/既Present assets等も実captureまたは補足に必要。
- native：実200 SYSTEM三本をprivate cloneで読んだreceipt。source_manifest_sha256、source_files三本の実SHA、errors[]、originals_unchanged=true、load_rc/unload_rc/privilege_restore_error=0を要求。`old_services`に保持する全serviceのpresent/root/subtree、`captured_named_values`に全key/name/type/bytes/raw_hex、`nodes`に実selected tuple、`bt_start.selection`、`post_ownership_policy360`、`module_identity`、`policy_sha256`を保存する。`new_service.present=false`は新service200の実不在を意味する。旧read196のnew_service196の結果を200不在証明へ流用しない。
- capture_contract：下記seal helperで実receiptから名前集合/件数を固定する。18/125を新captureへ流用しない。過去templateのservice/値の名前集合は全て新観測集合に含める。新実captureの値を過去の値へ上書きしない。nativeの観測範囲に追加service/値がある場合も全件を保持する。
- template_policy：canonical196の実署名policy。旧実行契約のtemplateとして使い、baselineのSHA/version/count権威には使わない。
- actual_package_rows：実fresh200に選択されている全13roleのpackage行JSON。旧prepare196と同じnames/directory/manifest/source/date/version/extra_members形式。mainの実署名packageだけを指定する。保持trioのCAT所属はSCM署名build時にも確認する。
- boot_witness：同fresh200 manifest/Recovery bootに結合された実boot読取り証拠JSON。source_manifest_sha256/recovery_boot_id/read_only=true/boot_sha256を含む。templateの既存matched bootと一致する場合だけ保持し、boot/GPT書込みを追加しない。未読取り値を作らない。
- mutable_baseline：実同captureの旧D3D log inventory。source_manifest_sha256/recovery_boot_id/filesを含む。filesはlog basenameから実path/sha256参照へのmap。旧logを消さずprefix byte/hash比較を維持する。
- GPU368_handoff：main-signed-handoff.json。status/compiled_source/manifest/SYS/UMD SHAを確認する。

新native captureの収集名集合が不足していれば、実同bootで補足読取りする工程が必要。seal helperは読取りreceiptを作ったり不足snapshotを合成したりしない。新service名/新named値の実取得はmain担当。fresh200実データによるfinalize成功はまだ未確認。

## 同bootのクライアント順序

1. idx0：既署名consolelauncher363 ab17ed4c... → present356 eaa30760...。launcher363はchild終了をWaitForSingleObjectで待つ。
2. idx1：既署名dxg-global-diag198.exeを1回。SHA9be80b8176696302a0e4408e6871d41792c780a37bcf6caca817c817b97061ac、source a5d8048c3e1572015d09bd308e4304333d9a53f4。signed-manifest 1dcacdd1...とmain-rt81-import-proof 89613783...を固定照合し、14imports全present/missing[]を確認する。元signed-manifestのRT81_exe_imports_verified=falseを上書きせず、後続のmain proofを別に保持する。

arg1は固定絶対prefix `C:\Windows\System32\MT8167CombinedFollowup200\logs\global-diag368`。新service directory内で各fileをCREATE_NEWする。別prefixへのfallback/再試行はない。helperはAPI Type9、raw_capacity262144。負tag23、enum/escape失敗、空rawはGPU成功として扱わない。

SCMの旧phase1-last-only制約は新GPU368専用フラグ既定offで分岐する。Peripheral境界snapshot/flushはidx0前に1回だけ。idx0の結果はgpu-result.bin、idx1はglobaldiag-result.binへ分離し、個別PID/exit/0x220/0x221を保存する。Present既存readiness条件は緩和しない。GPU開始不良時はPresentをcreated0/ERROR_NOT_READYとして記録し、read-only globaldiagは同bootで1回取得する。Presentが開始できた場合はその終了後に進む。結果が負でも成功へ置き換えない。boot fence・attempt marker・service自己無効化は既存のまま。

## main用コマンド（実fresh200取得後のみ）

既Windows checkoutはこのbranchの公開固定HEADにし、sourceとservice/mediaは同HEADを使用する。root相対pathを維持したprivate入力workspaceを用意する。変数は既実在pathに設定する。SDK再setup、GPU/PCO再buildは不要。

```powershell
# 実fresh200/native取得・hash固定後。補足があれば--supplement/--supplement-sha256を追加。
python -B tools/recovery/seal-gpu368-capture200.py --workspace "$Workspace" --fresh "$Fresh200Manifest" --fresh-sha256 "$Fresh200Sha" --native "$Native200Receipt" --native-sha256 "$Native200Sha" --out "$CaptureContract200"
if ($LASTEXITCODE) { throw 'Actual capture seal rejected' }

# CaptureContract200の実SHAをprivate mapへ固定してからSCMの1TUだけ生成。
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ci/build-combined200-service.ps1 -Mode Service -Workspace "$Workspace" -PackageMap "$Map200" -OutputDirectory "$PWD/build/gpu368-service200" -KitHeaderRoot "$ExistingKit81Headers" -LibraryRoot "$ExistingArmUmLibraries" -SigntoolPath "$ExistingSignTool"
if ($LASTEXITCODE) { throw 'SCM build/sign failed' }
```

続けてmainが既native SYSTEM準備入口へ、ServiceName MT8167CombinedFollowup200、新service exe SHA、実fresh200三本のSHA、生成peripheral-preservation-plan.jsonとそのSHAを渡す。`prepare-offline-system-service.ps1`の従来必須引数は維持する。workerは保存原本をloadせずcloneだけを操作し、全service subtree/全named値をpatch前後で照合する。

媒体は既CLIに `--combined200-profile` を追加する。他profile flagとは排他。mainが既comparator/base/toolsを指定する。

```powershell
python -B tools/winpe/build-private-peripheral-batch-media.py --combined200-profile --enable-peripheral-batch --workspace "$Workspace" --map "$Map200" --service-package "$Service200Package" --prepared-system "$Prepared200System" --prepared-result "$Prepared200Result" --prepared-sha256 "$Prepared200SystemSha" --comparator "$ExistingComparatorPackage" --comparator-manifest-sha256 "$ExistingComparatorManifestSha" --base "$ExistingBase" --tools "$ExistingTools" --source-commit "$FixedRecipeHead" --out "$NewMedia200Directory"
if ($LASTEXITCODE) { throw 'Media generation rejected' }
```

mediaは新schemaを認識するprepare200からpolicyを再生成し、署名SCMのpolicy/header/EXE/manifest、native cloneの実report、正確なcapture inventoryを結合する。旧196署名SCMを200へ再利用しない。実fresh未取得の現段階ではこのコマンドを実行しない。

## 回収・未検証

`collect-combined200.sh <main-seal.json> <pipeline-state.json> <root内新規out>` はmainがRecoveryへ切替済み・root化済み・既/tmp/rt-ntfs3g利用可能な時だけ使う。collector自身はreset/reboot/新prebackup生成をしない。service directoryの全bin/txt/logをread-only回収する。特に `logs/global-diag368-response.bin`、`-raw.bin`、`-records.txt`、globaldiag-result.bin、gpu_globaldiag.stdout/stderr、SCM記録を同boot/PIDと結合する。ファイル欠落や空rawを成功と扱わない。既Present collectorも1回呼ぶ。新read-combined200-post.ps1は2client schemaと実inventory名集合を検証する。

source-level検証は実署名368 manifestの受理、旧schema/混合UMD/成功主張/旧INF拒否、Python構文、canonical196入口byte保持、専用dual-phase1分岐、collector bash構文。Windows SCM compile/link/署名、fresh200 finalize、媒体生成、実GPU/DWMは未確認。RT/SRV限定profileとblend/scissor/部分copy未完了は変えない。

既canonicalのoptional legacy USB20が実captureで不在の場合だけ、旧SYS実byte・旧署名manifest一致を保持して扱える。そのversionはhistorical package由来と明記し、現在選択されているversionとは主張しない。parent/HCDの新HWIDをlegacy slotへ差し替えない。

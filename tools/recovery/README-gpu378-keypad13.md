# 実227 → run228 → post229

基点9072458f、共通200/377 generator/SCM/media/once-only controllerを再利用する薄variant。更新はrender378＋keypad1.3（mask1152、C install_orderの10→7）。UMDはwire378の実候補へ明示置換し、旧byteをbaseline_witnessesに保持。FW/transport372/WiFi1031/battery1.8/client375/boot98de/GPTは保持。予定restart1、Present/helper各1、terminal後Windows保持、collectは別入口。driver署名や実機成功はこの入口で生成しない。

## 明示入口

- generator: `prepare-gpu378-keypad13-inputs.py --workspace ROOT --map ACTUAL_MAP --out NEW_OUT --finalize`
- SCM: `build-combined200-service.ps1 -CaptureNumber 227 -Gpu378Keypad -Mode Service ...`
- native: 既`prepare-offline-system-service.ps1`、service `MT8167CombinedFollowup227`だけ。
- media: 既`build-private-peripheral-batch-media.py --gpu378-keypad-profile ...`
- controller: `run-gpu378-fresh227.py`。seal schema `GPU378_RUN228_MAIN_SEAL_V1`、既seal/pins/once-only/terminal規則を維持。

## mapの入力形

schema `GPU378_FRESH227_FINALIZE_INPUTS_V1`、profile `combined-features-fresh227-gpu378-keypad13-client375-v1`、capture_number227、hardware_owner main、main_execution_approved true、install_targets `[render,keypad]`。承認はユーザーの既実行許可に基づくmain入力であり自動付与しない。

以下はすべて `{path,sha256}` の実file ref。

`fresh, supplement, native, capture_contract, template_policy, retained_client_policy, actual_package_rows, GPU378_manifest, GPU378_handoff, keypad13_manifest, keypad13_handoff, boot_witness, mutable_baseline, globaldiag_manifest, globaldiag_import_proof`

fresh227 MF43050ae6/Recovery0478d105、native960e3163、assetsd27c0b5b/mutabledeacfdaf8を固定。template_policyは実run226の377 policy、retained_client_policyはそこで参照済みの元signed375 policy。前者の実client tableと後者から再検証したtableの一致を必須とする。全13actual_package_rowsはreceiptの実選択/SYSへ照合してから2roleだけadoptする。

capture contract schema `GPU378_FRESH227_CAPTURE_CONTRACT_V1`。実fresh/native SHA、Recovery、重複source_pathを除いたfile_count、native old_servicesのservice_names、captured_named_valuesのnamed_value_idsを持つ。boot witness schema `GPU378_RETAINED_BOOT227_V1` は旧225witness/run226 seal/resultへ結合。main保持申告をphysical readや連続独立監視へ読み替えない。

`gpu378-keypad13-accepted.json` は公開sourceのexact signed manifest/main review/source/SYS/UMD lock。未署名・未取得状態はgpu378=null/status pendingとして拒否できる。現在はmainから正式受領した6ff34226/e215c701とc2a7c929/3414de13を固定している。未取得refsを埋めて通す入口はない。

## keypad CPと保持ガード

基点のlive `preserved()` はservice root値をhash比較し、全subtree hash比較ではない。CPは子の `Mt6392Keys\\Parameters\\NativeCheckpoint` だけなのでrootガードの除外は不要。root/named値・offline subtree fingerprintは維持し、service全体を除外しない。

新profileのmacroだけで追加するCP validatorはREG_BINARY epoch16B/record80B、B01..B24、magic/version/stage/kind/予約領域、子keyなし、record時刻とepochの大小、前後epoch不変を検査する。shape確定前にrecord wordを参照しない。古いepochの構造正常recordはstaleと区別し今回到達証拠にしない。CP欠落は未観測であり入力成功ではない。

post229は既同load readerに `-IncludeButtonsDeviceBindings -IncludeKeypadCheckpoint` を指定する。固定namespaceのraw REG_BINARYとdecoded stage/status/aux/epochをreceiptへ保持する。異型・短い値はschema errorとして保存し、他デバイスの回収を捨てない。extra load0、writer0。新service229absent＋AdditionalServiceNames227を同loadで確認する。

既keypad_hid_bindings(host DevInst)も新profileのphase1で1回呼ぶ。CM ancestry一致のcollection/serviceを既helperで取得する。host STARTED/noProblem（既run226 event202/203/204）とは別の証拠。

## 診断前の画面idle設定

既正規Power helperのみ使用。phase1のpreserved()と実installed identity検証後、run_clients前に1回適用。終了時は同scheme/AC/DC VIDEOIDLEをreadbackするだけで、二度目の保存・書込みはしない。旧profileの適用時点は維持。

Power helperは元scheme/AC/DCを保存してから0へ設定、active scheme再確認とreadbackを行い、部分失敗時は既rollbackを試みる。既0x2b0..0x2b6に実API/rollback結果、0x2b7にbefore=0/after=1、0x2b8にidentity guard結果を残す。API失敗は非ゼロのままであり、画面点灯・GPU成功・原因確定を主張しない。identity guard失敗時は書かず、適用未成功の終了readbackはERROR_NOT_READY。legacy SetThreadExecutionState fallback、pollingによる設定書込み、追加restartはない。

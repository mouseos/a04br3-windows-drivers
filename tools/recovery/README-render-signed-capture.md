# 任意actual captureへ結合するrender-only GPU380更新

7767f9faのshared200入口を再利用する任意選択adapter。
既定では無効で、既存profile/generator/driver/EDK2を変更しない。
GPU380の実署名manifest・main独立review・Windows certificate reportは
`render-signed-capture-accepted.json`で固定済み。署名実行は行わず、保存済み報告の
同byte結合と全15leaf/INF/HWID/ClassGuid/source/versionを検査する。
候補lockのartifact承認は、更新実行や実機描画の成功承認を意味しない。

初版時点の`render-signed-capture.pending.json`はcapture番号・fresh等がnullの準備用で、
schemaもFINALIZEではない。実取得状況は各manifestで確認し、233のreceiptを235等へ
改名/コピーして通さない。
未完成mapから最終policy/header/native/mediaを生成しない。

## 最小の最終map

実取得後だけschemaを`SIGNED_RENDER_CAPTURE_FINALIZE_V1`へ変え、次を全て固定する。
参照は原則`{"path":"workspace内の相対path","sha256":"実64hex"}`。
未確定・zero SHA・余分なcomponent fieldは拒否する。

- `capture_number`: 実235..999、`hardware_owner: "main"`、
  `main_execution_approved: true`。これらは実データ/実行判断が揃ってから設定する。
- `fresh`: captured-unmounted/read_only/unmounted/errors空の実manifest。
  shared200と同じ`private-windows-rt-prebackup-run<N>/manifest.json`。
- `native`: fresh SHA/boot ID/capture番号へ一致する一度のread-only inventory。
  load/unload/restore0・原本不変・service全件・新service不在を要求する。
  今回の作業ではnative取得を行わない。
- `supplement`: 同fresh SHA/boot IDの実補足capture。全file bytes/SHAを検査する。
- `capture_contract`: `SIGNED_RENDER_CAPTURE_FRESH<N>_CAPTURE_CONTRACT_V1`。
  実fresh/native SHA/boot ID、capture番号、service_names、named_value_ids、
  combined file_countを固定。新serviceは`MT8167CombinedFollowup<N>`。
- `template_policy`: 直近の実media policy。全既存service/named情報とinput pinsを保持。
- `actual_package_rows`: 全13roleの実package行。実SYS/service/node/選択INF版・日付と照合。
  wifi1033/keypad14、現在transport、他12roleをそのまま保持する。
- `mutable_baseline`: 同fresh/boot IDの実clear/umdログbaseline。
- `boot_witness`: `SIGNED_RENDER_CAPTURE_RETAINED_BOOT_V1`。
  source_manifest_sha256/recovery_boot_id/boot_sha256、下記2ref、
  physical_read_this_capture=false、
  no_intervening_boot_or_gpt_write_reported_by_main=true。
  物理boot readや連続独立監視を実施済みと表示しない。
- `last_boot_seal` / `last_run_result`: 最後の実main seal/result。
  seal/transaction/matched boot SHAの一致とboot/GPT書込み0を要求する。
  上記witness内の同名refも一致させる。
- `additional_service_names`: 実inventoryから省略しない過去controller等の一覧。
- `GPU_candidate` / `main_review`: accepted lockの実refと完全一致。
  candidate row/UMD/FWは最終packageからadapterが導出し、旧packageのextrasを流用しない。
- `retained_client_policy`: 実379 signed pairを使用した保存済みpolicy。
  pending templateには実run232のpolicy参照を収録してある。
- `client_pair`: 既379 formal signed pairの原receipt。launcher/evaluator・proof・署名を
  既schema adapterで検査し、actual captureにも両EXEが同byteであることを要求する。
- `globaldiag_manifest` / `globaldiag_import_proof`: 既署名globaldiag198の実ref。
- `install_targets: ["render"]`、`planned_reboots: 1`、`filter_root_transitions: []`。

新規actual237以降だけ、`unattended_collection_reboot: true`を任意追加できる。
この場合は診断終了後のflush・サービス停止・同run/PID/policyの証拠を確認して
通常の再起動APIを一度だけ要求し、自動回収へ進む。旧世代へは適用しない。
上記とこの任意field以外は最終mapで拒否する。USB合成は別componentとして扱う。
`capture+1`をrun、`capture+2`をpost captureとするが、番号の算出は取得証拠ではない。

## keypad14の同load境界

SCM headerは既存`A04_KEYPAD13_CHECKPOINT=1`に加え、今回profileだけ
`A04_KEYPAD14_CHECKPOINT=1`を設定し、元の80B/schema/LoadEpochとB25..31の
固定名・kind・整数pack制約を保持する。Mt6392Keysのroot全免除は追加しない。
既存`keypad13-checkpoint-policy.h`をbyte変更せず使用する。

次の正当なnative inventoryで既入口へ`-IncludeKeypadCheckpoint
-IncludeKeypad14Boundaries -TimingCaptureNumber <N>`を明示する。
新switchは既定OFFで、旧reader/callerは変更しない。
`read-keypad14-checkpoint-loaded.ps1`は既承認a13 readerの関数名だけを分離したもの。
receiptのsource_recipe_filesにその実SHAが必要。stale/unknownは今回の成功としない。
この文書は現在のWindows holdを解除したり新hive loadを指示するものではない。

## 既入口への接続

データ完成後の入口のみ示す。今回これらのbuild/patch/media/device操作は未実行。

1. `render-signed-capture.py --workspace ROOT --map FINAL --out NEW --finalize`
   はshared200でpolicy/headersを生成する。
2. 既`tools/ci/build-combined200-service.ps1`の`-SignedRenderUpdate
   -CaptureNumber <N> -Mode Service`と既SDK/libs/SignTool引数を使用する。
3. 既`prepare-offline-system-service.ps1`はこのprofileとaccepted lockだけを厳密admitし、
   native SYSTEM patch/verifyの既保持検証を継続する。新native処理frameworkはない。
4. 既`build-private-peripheral-batch-media.py --signed-render-profile`へ同mapを渡す。
   native結果validatorも全policy再生成で照合する。新mediaでは
   `--winpe-lease-manifest/--winpe-lease-manifest-sha256`と
   `--atomic-replace-manifest/--atomic-replace-manifest-sha256`が必須。
   1800秒のWinPE専用watchdogと、実kernelの期限照会を行うatomic publisherを組み込む。
5. 既`run-gpu368-fresh200.py --signed-render-capture <N>`へmain作成の
   `SIGNED_RENDER_RUN<N+1>_MAIN_SEAL_V1`と既引数を渡す。
   generator/controller/input pins・once-latch・terminal hold・既timeoutを維持する。
   新actual237以降の無人試験ではmapへ上記opt-inを加え、SCMへ`-CollectionReboot`、
   sealへ同collection CONFIG・WinPE lease・atomic packageの実pinを記録する。
   起動先には`run-unattended-signed-render.py --signed-render-capture <N>`を使う。
   詳細は[無人回収入口](README-unattended-signed-render.md)を参照。

render専用選択maskは1。しかしshared200の固定13role表ではrenderはbit10なので
実`A04_PERIPHERAL_INSTALL_MASK`は1024（0x400）。mask1を13role表へそのまま渡すとrtcに
なるため、両者を`render_update_mask:1` / `install_mask:1024`と明示して区別する。
期待値はrender1role、restart1、boot/GPT0。旧既定は最後にWindows hold。
新規の無人opt-inは診断後の回収用再起動とRecovery回収までを要求する。

既diag2だけ（Present launcher/evaluator一組とglobaldiag198各1）を使い、
追加diagnostic EXE起動を増やさない。pre-clientの既Powerhelperはpreserved成功後だけ。

## UMD/FWと前後の検査

更新前のUMD/FWはactual capture/旧package manifest/旧extra行の3者一致が必要。
更新候補extraは最終GPU380packageの実UMD/FWだけ。
変化するextraのpre-copy guardは旧capture baseline witnessを使う。
インストール後のpreserved_files/preserved_sourcesは新package bytesへ結合する。
これにより旧UMDを新候補として検査したり、新UMDをpre-copyで要求する混同を防ぐ。
現在transportを含む他12roleと、package15leafのうちrender INF/SYS/CAT/UMD以外は保持。

## 検証範囲

`test-render-signed-capture.py --workspace ROOT --out NEW_ROOT_LOG_DIR`は実380の
候補読取り、実233と未確定入力の拒否、全旧controller契約の同一性、
旧generator/旧readerのbyte保持、新14readerの承認source一致を確認する。
未来captureの成功fixtureやfinal policyを捏造しない。
Python/PowerShell parseとhost契約の成功は、未取得actual235での最終再生成・
ARM SCM compile/sign・native patch/verify・media生成・実機動作成功ではない。


## 初回の期限リセット確認

`tools/winpe/build-winpe-lease-qualification.py`は同じ署名lease/atomic packageから、
通常Windowsの更新・明示的なWindows volume mapを要求しない資格確認イメージを作る。
startnetは`atomic-stage-replace.exe --check-lease`を実行してCRC付きUART報告を送り、
カーネルの1800秒SWRSTを待つ。ホストUART resetや通常のwpeutil rebootで成功を代用しない。
資格確認用WIM/ESPの生成は実機reset/Recovery復帰の成功を意味しない。
これを実機で試した場合はRecovery boot IDが変わるため、後続の更新用fresh/nativeも
その新しい実機状態から取得する。古いfreshをそのまま更新に使わない。

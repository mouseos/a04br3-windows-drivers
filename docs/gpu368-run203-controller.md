# GPU368 / fresh202 → run203 → capture204

main実行専用。201はpreflight1306、DIF0/clients0。利用者報告では同bootの旧USBInstall198が
Start2→4へself-disableしたactual200/202差分が原因。201候補の再実行ではなく、
Averroesのnewservice202/profile202をmainがsealで固定し、別run203として実行する。

## 共通実装と入力

`run-gpu368-fresh202.py` は薄い入口だけで、201のengineに `run203` を渡す。
`Observation` は201基点b3c10a8bと同一で、CRC decoder・renderのみ・restart1・
Present index0/helper index1各1回・same-boot run/PID/flush/terminal条件は共通。
任意run/profileへ転用できる汎用設定は作らず、既知201/203だけを許可する。
新boot/GPT書込み0、EFIのみ。hardware_successはfalseのまま回収する。

`gpu368-run203-seal.pending.json` をroot内のprivate seal先へcopyし、mainが全実path/SHAを記入する。
未確定テンプレートは実行不可。nativeprofile202側のcommitをコードへhardcodeせず、
`source_tree/profile_source_commit` をmainが指定し実HEADと照合する。
controller/collectorをnativeprofile202 treeへmergeする必要はない。

| 項目 | run203固定値 |
|---|---|
| schema / transaction | GPU368_RUN203_MAIN_SEAL_V1 / combined-run203 |
| policy profile | combined-features-fresh202-gpu368-v1 |
| service / data directory | MT8167CombinedFollowup202 |
| fresh_manifest | port/xnu/out/private-windows-rt-prebackup-run202/manifest.json |
| capture_output | port/xnu/out/private-windows-rt-prebackup-run204 |
| diagnostics_output | port/xnu/out/private-windows-rt-gpu368-run203-diagnostics |
| --output | port/gsi/logs/ios-feasibility/windows-rt-combined-run203-runtime |
| 排他latch | port/gsi/logs/ios-feasibility/gpu368-run203-executions/combined-run203.json |

sealの `controller_path` は本branchの `run-gpu368-fresh202.py`、`controller_engine` は
同checkoutの `run-gpu368-fresh200.py`、`diagnostics_collector` は同checkoutの
`collect-combined200.sh`、`collector_present_helper` は同checkoutの `collect-present356.py`。
全て実SHAで `pinned_files` に含める。collectorは既知seal/service組を選び、
result.transactionの一致も要求する。collectorの全処理複製はない。
`capture_tool` と `crc_decoder` はmainが実績sourceの実path/SHAを指定する。
その他media manifest、WIM、ESP、policy canonical SHA、normal ESP、fresh202の実manifest/bootID、
既target/reset/ntfs3gも201と同じseal検査へ通す。fresh200混在は拒否する。

## main実行コマンド

root内の実sealとそのSHAをmainが設定してから使う。以下は端末操作を実行した記録ではない。

```sh
ROOT=/media/yuma/e109b1f2-7a0a-4242-ba25-6bccbde011b1/work/Android/MediaTek/benesse/TAB-A04-BR3
CONTROLLER="$ROOT/windows-rt/a04br3-windows-gpu368-run203-controller/tools/recovery/run-gpu368-fresh202.py"
OUT="$ROOT/port/gsi/logs/ios-feasibility/windows-rt-combined-run203-runtime"
# MAIN_SEAL と MAIN_SEAL_SHA はmainが実media完成後に固定する。
python3 -B "$CONTROLLER" --workspace "$ROOT" --seal "$MAIN_SEAL" --seal-sha256 "$MAIN_SEAL_SHA" --output "$OUT" --validate-only
python3 -B "$CONTROLLER" --workspace "$ROOT" --seal "$MAIN_SEAL" --seal-sha256 "$MAIN_SEAL_SHA" --output "$OUT"
```

実fresh202 verify→今回tokenのCRC COMMITTED/release0/exit0→normal ESP→
PB286(run)/287(PID)→PB288 flush0・PB207=3010・PB10b=3010→予定restart1→
新bootに再結合→Present/helper→同boot flush0/terminal→Recovery→actual capture204と補足回収。
失敗terminalもflush0が揃えば回収し、timeoutだけを理由にresetしない。
201のresult/latch/capture202へ書かない。203 latchも自動削除・再試行しない。

## 検証範囲

`python3 -B tests/gpu368_run201_controller_test.py` と
`python3 -B tests/gpu368_run203_controller_test.py`。
201とのObservation AST一致、203 sealのhost fixture受理、201/fresh200/service200/capture202混在拒否、
collector202選択・transaction混在拒否を検証する。shellは `bash -n` まで。
実mediaに対する最終seal検証と実機処理はmainのみ。本作業はVM/runner/実機操作0。
Wi-Fi source e11e9d37とUSB公式PS修正版5c392は保存し、変更していない。

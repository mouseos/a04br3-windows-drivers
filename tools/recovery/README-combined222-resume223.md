# 拒否済run222の限定resume223

これは既CRC拒否（旧seal f3b184…、release0/exit6）だけの一度限りの入口。
service/policy/nativeは実C922のまま、追加compile/sign/hive loadは行わない。
SOFTWARE.LOG1の差は記録するが、同fileやSOFTWAREをコピー・復元しない。

## 1. host側の証拠sidecar

作業rootから次を実行する。deviceアクセスなし、既存出力は拒否。

```sh
python3 -B windows-rt/a04br3-windows-client375-combined/tools/recovery/combined222_rebind223.py \
  --workspace "$PWD" \
  --out "$PWD/port/xnu/out/windows-rt-gpu375-combined221-inputs/rejected222-rebind223.json"
```

旧seal/latch/result/prepare UART/CRC report、実221/223の各42file、
実署名service manifest/EXE、native result/candidate SYSTEM、旧witnessをpinする。
41/42一致とSOFTWARE.LOG1だけの相違を明示する。6fileのSYSTEM/DRIVERS trioは同一必須。
保護30file中24件は新捕捉でも一致、捕捉外の旧launcher/presenter/UMD/FW/dxgi/RenderSingleGpuは
修正mediaのWinPE pre-copy比較を必須のまま残す。未armはSYSTEM同一と旧C922入力に結合する。

## 2. mainによる新mediaとseal

既media CLIを新規outへ実行し、`--gpu375-combined-profile`と
`--service-source-commit c922efe5227b582801acd2ddba60c6fc823cd0b3`を指定。
`--source-commit`は今回の公開tooling commitを指定する。
旧map、署名service、native candidate、policy、旧221 backupは変更しない。
新WIM/ESPのembedded fileをmainが照合し、新規sealを別pathに生成する。

新sealは既schema `GPU375_COMBINED_RUN222_MAIN_SEAL_V1`を使用する。
`fresh_manifest`/SHA/`recovery_boot_id`は元221由来のpolicy出自として保持。
新Recoveryの実223はsidecarでのみ結合する。以下を追加する。

- `baseline_reject_resume`: sidecarの実`{path, sha256}`。
- `rebind_helper`: 公開`tools/recovery/combined222_rebind223.py`のroot相対path。
- `pinned_files`: sidecar自身、helper、sidecarの`refs`全件を既pin集合へ追加。
- `capture_output`: `port/xnu/out/private-windows-rt-prebackup-run223-resume223`。
- `diagnostics_output`: `port/xnu/out/private-windows-rt-gpu375-run222-diagnostics-resume223`。

controller/source_tree/engine/collector/generatorは新tooling checkoutをpinし、
service-sourceはC922に分離する。新ESPは拒否済ESPと異なることが必須。
旧seal/latch/result/report/media、既223捕捉を変更しない。

## 3. 明示入口（mainのみ）

```sh
python3 -B windows-rt/a04br3-windows-client375-combined/tools/recovery/run-gpu375-combined-fresh221.py \
  --workspace "$PWD" --seal "$NEW_SEAL" --seal-sha256 "$NEW_SEAL_SHA256" \
  --output "$PWD/port/gsi/logs/ios-feasibility/windows-rt-combined-run222-runtime-resume223" \
  --resume-baseline-reject --validate-only
```

上記はdevice操作なし。mainが実行する場合のみ`--validate-only`を外す。
preflightは実223 manifest/SHA/Recovery boot IDで既capture verifyを実行する。
通常入口へresume sealを渡すことも、resume指定へ通常sealを渡すことも拒否。
専用latchは`gpu375-run222-executions/combined-run222-resume223.json`を`open('x')`で作成。
二度目のresume、旧latch削除、無条件retryは許可しない。

terminal後Windows保持、予定restart1、Present/helper各1、boot/GPT0は既engineのまま。
後続のcollect-onlyには同じseal/outputと`--resume-baseline-reject --collect-only`を指定する。
その際も元223捕捉は上書きせず、新規`*-resume223`へ回収する。

# GPU368 terminal後のWindows保持と明示回収

共通controller `run-gpu368-fresh200.py` とrun203入口 `run-gpu368-fresh202.py` は、正常terminalと失敗terminalの両方でWindowsを保持して終了する。初回installに必要な既存の一回の再起動は維持する。GPU/driver/profile/native/mediaは変更しない。

terminal後の既存settle期間は遅れて届く障害の確認に使う。自動Recovery移行の猶予時間ではない。実panic/Unhandled Exception、想定外restart、再起動根拠不足は従来のRecovery/回収経路を維持する。同じUART読み取り内でterminalに続いた障害も検出する。観測timeoutやflush未確認を、成功terminalまたはreset許可へ変換しない。

保持状態は `result.json` の `status=terminal-windows-held`。失敗codeはそのまま `terminal.code` に保存し、GPU機能成功を意味する `hardware_success` は常にfalseとする。`terminal-proof.json` は同candidate/seal、同bootのservice run/PID/flush、UARTのSHA256と元のcollection境界を保存する。

## 将来の試験での操作

通常起動のCLI引数は従来どおり。更新したcontrollerをpinした将来用sealで実行する。現在203のresult/seal/latchを変更・移行する処理はない。旧sealのcontroller hashを緩和したり、既存latchを削除して同candidateを再実行したりしない。

```bash
# 新しい実行の通常CLI。terminal後はWindowsを保持して終了。
python3 -B "$controller" --workspace "$root" --seal "$seal" \
  --seal-sha256 "$seal_sha" --output "$runtime"

# ユーザーの接続試験などを終えた後、保存hold証拠を読取り検証するだけ。
python3 -B "$controller" --workspace "$root" --seal "$seal" \
  --seal-sha256 "$seal_sha" --output "$runtime" --collect-only --validate-only

# 回収を明示実行する。この操作だけがRecoveryへ移行して既存collectorを呼ぶ。
python3 -B "$controller" --workspace "$root" --seal "$seal" \
  --seal-sha256 "$seal_sha" --output "$runtime" --collect-only
```

`--resume-collect` は `--collect-only` の別名。`--output` は新規出力ではなく、そのcandidateの保持済みruntime directoryを指定する。

明示回収は既存latchとcandidate/seal/outputを照合し、保存UARTを既存Observation状態機械へ再入力してterminal/flush0を再確認する。install/EFI更新/Present/globaldiagは一切再実行しない。保持済みresult/proof/UART/latchは変更せず、回収の状態とログを `$runtime/collection/` に分ける。回収済み出力や部分回収を自動上書き・再試行せず、同じ入口の二重実行を拒否する。

`--validate-only` 単独は従来のseal検証のみ。`--collect-only --validate-only` はheld証拠まで検証し、directory作成・実機操作を行わない。

## ホスト検証

```bash
python3 -B tests/gpu368_terminal_hold_test.py
python3 -B tests/gpu368_run201_controller_test.py
python3 -B tests/gpu368_run203_controller_test.py
```

新試験は実controllerのmainを実行し、transportだけを明示mockへ置換する。正常/失敗terminalの保持、install失敗の保持、明示回収での再実行防止、原本不変、flush不成立拒否、証拠改変拒否、panic/想定外restart/根拠なしrestartの既存復旧経路を確認する。実機・VM・署名driverへの操作は行わない。

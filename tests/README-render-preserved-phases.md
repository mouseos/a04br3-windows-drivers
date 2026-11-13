# SCMのpreinstall OLD／postinstall NEW保持guard

`render-signed-capture.py` はsigned-render policyに明示的な `preserved_phase_guard` と `preinstall_preserved_files` を追加する。既存の `preserved_files` はpost-install／diagnostic用のNEW hashとして維持する。

preinstall表では、署名candidate extraのbaseline hashと実OLD witnessが一致するUMDだけをOLDに切り替える。保持FWとその他のfileは両表で同じhashを要求する。他fileの変更、OLD witness欠落・改変、preinstallへのNEW hash混入、phase契約欠落はgeneratorで拒否する。

生成headerは `A04_PRESERVED_PHASE_GUARD=1` と同じ順序・件数の `preinstall_preserved_files` を持つ。既存emitterを再利用するため、Windowsの相対／root相対pathのescapeもpost-install表と一致する。C側で件数のcompile時検査と名前の実行時検査を行う。新macroの既定値は0で、signed-render以外の既存profileは従来表の検査を維持する。

SCMの `transaction(diagnostic)` 入口では `preserved(diagnostic)` を使う。install phaseは0なのでOLD、再起動後diagnosticは1なのでNEWとなる。DIF後とdiagnostic内の追加guardは明示的に `preserved(1)` を呼ぶ。service fingerprint、named registry、keypad／vidpn guardは両段階で省略しない。OLDかNEWのどちらかなら許すfallbackはない。

実241のbaseline379／candidate380を使うhost検査は次のように実行する。

```bash
python3 -B tests/render_preserved_phase_host_test.py \
  --policy "$ROOT/port/gsi/logs/ios-feasibility/windows-rt-render241-preparation/host-policy241/peripheral-policy.json" \
  --work "$ROOT/port/gsi/logs/ios-feasibility/render241-phase-guard-check-new"
```

入力policyは読取専用。コピーしたfixtureを新generatorへ通し、OLD／NEWの実byte SHA256を確認して生成した表と、実Cの `path()`／`preserved()` をhost Windows API mockへ結合する。OLD-before／NEW-afterだけの通過、逆段階と第三hashの拒否、保持FW・他file・service・registryの異常拒否、既定OFF、旧新同hashの保持caseを検査する。実transactionの呼出し順も検査する。実Windows API・ARM EXE・実機・VMは実行しない。

既存のSCM/header/policy/media/receiptはこの修正で書き換えない。mainは新commitを正本へ取り込み、新outputへhost finalizeを再実行し、更新したpolicy/header/hash closureでSCM1TUを再compile・署名する。そのSCMと同じpolicyからnative hive clone patch、新media/sealを生成する。旧policy/headerへの手修正、旧署名EXEやpolicy hashの流用は禁止する。

host mockはpreinstallの誤NEW要求の修正を示すだけで、DIFの実完了やpost-installの実file置換・GPU成功を証明しない。DIFがfile置換を延期している場合も、NEW guardを緩めず不合格として回収・検証する。

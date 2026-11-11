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

## 旧拒否journalの保存退避（追加opt-in）

最初のpre-copy拒否でも`C:\a04backup\peripheral-batch-<policy token>`は残る。
通常guardはこのdirectoryが存在するだけで`JOURNAL_EXISTS_NO_CHANGE/exit4`にする。
このguardを無条件には解除しない。

mainがread-only回収した当該`JOURNAL.txt`がある場合に限り、media CLIへ
`--resume-rejected-journal <実回収file> --resume-rejected-journal-sha256 <実SHA256>`を追加する。
実回収前の仮file/hashで生成してはならない。固定combined375/C922/旧CRC拒否policyと
同tokenで、fileが厳密に`STATE=BASELINE_REJECTED_NO_CHANGE`のCRLF1行の場合のみ受理する。
media manifestの`rejected_journal_archive`へ実入力と退避先を記録する。

WinPEはvolume guard後、現在journalをembedded実回収witnessとbyte比較する。
退避先が未存在の場合だけ旧directory全体を
`peripheral-batch-<token>.baseline-rejected-<journal SHA先頭16桁>`へmoveし、
退避journalも再比較してから新journal directoryを作る。
旧journal/prepare-report等を削除せず、既存退避先へ上書きしない。
不一致、別state、旧directory不在、退避先既存、move失敗は拒否する。
既存の36pre-copy比較と候補書込み境界は維持する。

このmedia opt-inだけで消費済みresume223 latchを再開放しない。
次の実Recovery/捕捉/拒否証拠に結合した別の明示resume許可が必要であり、
旧latch/output/resultの削除・改名・上書きで代用してはならない。

prepare UARTはraw原本を保持し、既`STORAGE_TRACE`完全一致spanだけを除去したcopyを
`prepare-uart-storage-deinterleaved.log`へ保存してから既CRC decoderへ渡す。
chunk順序/長さ/全chunk CRC/whole CRC/terminal検証は変更しない。

## 実journal224取得後の明示resume224

実224 MF `bf5e2f6a…`、Recovery `21115fbe-6271-4497-bd1b-b14632aaf176`、
実journal35B `08c8a9e3…`、保存report3627B `ecc732ca…`を固定した入口。
旧223の消費済seal/latch/result/rawも検証し、storage完全一致span除去後の
全chunk/whole CRCが`JOURNAL_EXISTS_NO_CHANGE/release0/exit4`である場合のみ受理する。
実224全42fileは223と同hash/size必須。221とのSOFTWARE.LOG1差は保持する。

```sh
python3 -B windows-rt/a04br3-windows-client375-combined/tools/recovery/combined222_rebind223.py \
  --workspace "$PWD" --after-journal224 \
  --out "$PWD/port/xnu/out/windows-rt-gpu375-combined221-inputs/rejected222-rebind224.json"
```

新sidecarは`GPU375_RUN222_JOURNAL_REJECTED_REBIND224_V1`/
`resume_capture_number=224`。controller/collectorはhash固定sidecarからsuffixを選ぶ。
main sealは同じ`baseline_reject_resume`へこの新sidecarの実refを指定する。
別のCLI flagやprofile/policy更新は不要で、`--resume-baseline-reject`を継続使用する。

- 新media: `port/xnu/out/private-windows-rt-combined-followup221-media-resume224`
- 新seal: `port/gsi/logs/ios-feasibility/windows-rt-explicit-source375/main-deployment-contract-resume224.json`
- 新runtime: `port/gsi/logs/ios-feasibility/windows-rt-combined-run222-runtime-resume224`
- 新latch: `port/gsi/logs/ios-feasibility/gpu375-run222-executions/combined-run222-resume224.json`
- 新post: `port/xnu/out/private-windows-rt-prebackup-run223-resume224`
- 新diagnostics: `port/xnu/out/private-windows-rt-gpu375-run222-diagnostics-resume224`

media CLIは実`port/xnu/out/private-windows-rt-journal224/journal/JOURNAL.txt`と
そのSHA `08c8a9e3fd92a5c04ba40a73d5131491af4f34ac3f1379e8df154d362104c888`で
journal opt-inを指定する。新seal検証でもmedia manifestのjournal入力ref/tokenと
固定sidecarを再照合する。拒否済2媒体のESPそのものは受理しない。
元221出自/署名C922/native/policyは維持。preflightおよびinitial deploy gateは
live224 MFを使い、initial=Falseの通常Windows用deployは従来どおり。
旧run222/resume223のlatch/result/captureを変更しない。

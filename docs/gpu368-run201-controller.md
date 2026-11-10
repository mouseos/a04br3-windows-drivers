# GPU368 / fresh200の一回実行controller201とcapture202

実行ownerはmain。新boot/GPT書込み0、EFIの差分更新媒体と正常Windows ESPだけを配置する。
本source作成では実機・runner・VMを操作していない。

## main sealと実行

`tools/recovery/gpu368-run201-seal.pending.json`は入力欄の一覧であり、未確定なので実行不可。
map/service/media完了後にmainが実path/SHAを記入し、statusをvalidated-main-sealとして固定する。
未完成media、旧native197 receiptの改名、fresh202合成は行わない。

sealには以下を固定する。

- `source_tree/profile_source_commit`: 実profile/service/mediaに対応する正式source checkout。参照基点は6815927605d230a76a7f972394b7330e0b2cba5b。native200修正が入った場合はmainが最終sourceへ固定する。
- `controller_path`: 本branchの`tools/recovery/run-gpu368-fresh200.py`。
- `capture_tool`: profile sourceの`tools/recovery/capture-windows-prebackup.py`。
- `crc_decoder`: 実績ある`decode-audit-stream.py`の実path。profile tree内にない場合も、mainが実績sourceのpath/hashを指定する。controller独自decoderを作らない。
- `diagnostics_collector`: profile sourceの`tools/recovery/collect-combined200.sh`。
- `reset_script/target_tools/ntfs3g`: テンプレートに示す既存実装・実binary。
- `fresh_manifest/fresh_manifest_sha256/recovery_boot_id/serial`: 実fresh200。完了済みcaptured-unmounted/read_only/errors0と原本SHAを確認する。
- `media_directory/media_manifest_sha256/wim_sha256/esp_sha256/policy_sha256/prepare_token`: 実最終media。prepare tokenはcanonical policy SHAの先頭16桁uppercase。
- `normal_esp/normal_esp_sha256`: 既正常Windows起動ESP。新bootは書かない。
- `pinned_files`: 上記全tool/input file、media manifest、source helperなどの実SHA。controllerは実行前に全pinを照合する。

`serial`はfresh200とtarget-toolsのrt_target_serialの双方と一致する必要がある。
profileはcombined-features-fresh200-gpu368-v1、service/data directoryはMT8167CombinedFollowup200。
install_targetsはrenderだけ、planned Windows rebootは1、Present(client0)・globaldiag(client1)各1回。
GPU client900秒、helper60秒、absolute diagnostic batch1140秒。

```sh
python3 -B "$CONTROLLER" --workspace "$ROOT" --seal "$MAIN_SEAL" --seal-sha256 "$MAIN_SEAL_SHA" --output "$ROOT/port/gsi/logs/ios-feasibility/windows-rt-combined-run201" --validate-only
# mainが起動する実行。出力先は新規でなければならない。
python3 -B "$CONTROLLER" --workspace "$ROOT" --seal "$MAIN_SEAL" --seal-sha256 "$MAIN_SEAL_SHA" --output "$ROOT/port/gsi/logs/ios-feasibility/windows-rt-combined-run201"
```

global latch `port/gsi/logs/ios-feasibility/gpu368-run201-executions/combined-run201.json` を排他的に作成する。
log directoryやseal名を変えてもrun201を自動再実行しない。latchを自動削除しない。

## 実行順序とUART境界

1. 既Recovery・NTFS unmounted・実fresh200 verifyを確認して、WinPE差分ESPだけをEFIへ書く。
2. 今回tokenのA04AU2 CRCを既decoderで検証。COMMITTED/release0/exit0が各1件揃った場合だけ先へ進む。
3. 既reset/fastbootの実装でRecoveryへ戻り、正常Windows ESPだけをEFIへ配置する。
4. current bootのEBS/既GPU D0 markerとPB286 run ID・PB287 PIDを結合する。raw UART原本と既知A04WIN3形式だけを除いたdeinterleaved logを別々に保存する。
5. install phaseのPB288=0、PB207=3010、停止waiter PB10b=3010と実BOOT_REASONを要求する。予定Windows restart1回だけを認める。fastbootに止まった場合は既fastboot rebootで一度だけresume。直接Windowsへ進む場合も同じ新bootの診断phaseへ結合する。
6. 診断phaseでPresent id0とhelper id1のPB220 PID/PB221 resultを各1回収集する。重複client startは再試験として拒否。
7. 同phaseのPB288=0とPB207 terminalを要求する。terminal0には両clientの実PIDとresult0も必要。失敗terminalでも停止・flush境界が揃えば失敗ログ回収へ進む。
8. source recordのログflush-before-UARTを根拠に、短いseal指定settle後に既Recovery経路へ戻る。actual capture202とbounded diagnosticsを回収する。

PB207はGPU機能成功ではない。driver install、Presentの評価、globaldiag API結果、GPU機能成功は
capture202/native receipt・各clientの実file/registry evidenceをmainが独立照合して判定する。
controllerは終始hardware_success=falseを維持する。

## 失敗とtimeout

CRC未完了／prepare失敗では正常Windowsへ進まない。timeoutだけを根拠にresetしない。
install900秒、診断boot開始待ち180秒、実service開始後absolute1140秒を上限として観測する。
Present開始で最大930秒（900秒＋終了記録猶予）、helper開始で最大90秒を設け、absolute上限を延長しない。
PB terminalがあっても同boot flush0がなければmain reviewで停止する。

既知BUGCHECK/Unhandled Exception、実際に観測された想定外再起動は具体的な停止根拠として
既reset/Recovery経路でfailed-boot collectionへ進む。これはsafe-flush成功を意味しない。
同候補のreplayや追加GPU試験はしない。

## capture202

自動collectionの出力先は固定する。

- `port/xnu/out/private-windows-rt-prebackup-run202/manifest.json`: actual Recovery capture。全指定SYS、SYSTEM/LOG1/LOG2、既engine/sd stateを既capture toolでread-only取得。
- `port/xnu/out/private-windows-rt-gpu368-run201-diagnostics/`: 既collect-combined200.shを再利用し、service/log、Present出力、globaldiag response/raw/records、UMD、event/setupapiなどをRO/norecoverで取得。
- controller logs: `capture202.log`, `collect-combined200.log`, `result.json`。全実書込み/reset/resume logは既flash.logにも追記。

/tmp/rt-ntfs3gへ既binaryを送り、Windows filesystemはro,norecover,nosuid,nodev,noexecで読む。
capture202のcaptured-unmounted/read_only/errors0とdiagnostics indexのwindows_unmountedを実測確認してから
`capture202-and-diagnostics-ready-for-main-review`とする。欠落file・途中capture・timeoutを成功に置換しない。
sourceのread-combined200-post.ps1を使うVM native replayは本controllerから起動しない。main/Averroesが後で実capture202を入力にする。

## host検証

実Observationクラスを使い、正常install/restart/2clients、flush欠落、余分な再起動、
Present readiness失敗＋helperのみ、偽terminal0、waiter欠落、client重複、未確定seal拒否を確認する。
adb/fastboot/reset/VM/driverは実行しない。実際のmap/service/mediaとmain sealでのend-to-endは未検証。

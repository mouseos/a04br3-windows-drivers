# run155 matched thermal1117（fresh154待ち・非arm）

基点sourceは凍結153最終beb3edcc。153適用中の媒体/sourceに変更しない。

`tools/recovery/prepare-combined155-draft.py --workspace ROOT --out ROOT/port/xnu/out/windows-rt-combined155-thermal1117-draft`
でmain seal/pinsの全14memberを実byte照合し、`contract-DRAFT.json`だけを生成する。
これは正式package-mapではなく、既存builder/native armへ渡してはいけない。
`reject_arm()`は常に拒否。fresh154未取得の値はnullのまま保持する。

## 正式化する差分

- 同一updateにparent1117（source b2c1a313、SYS bbced952）とboot2e6cba6a（UEFI b4ce1992）を固定する。boot書込み必須。leaf1.10および他driverは実actual154から保持する。
- 親だけDIF（semantic role parent/index6/mask0x40）、予定restart1。BT初回登録なし。12target snapshotと既存SystemRequiredは保持。
- builtin温度IOCTL0x226810/28Bを1回、その完了後（測定失敗時も）status542c7393 `--status`、0x226814/64Bをphase0で1回（10秒）。他の機能clientは0。
- status stdoutはlogs/thermal_status.stdout.txt、binaryはclient cwdのthermal-ioctl-status-v1.bin。exact completion/cal/query/measure/UART NTSTATUSを保存し、exit0を温度成功と解釈しない。
- install900秒、boot180秒、最初phase1 PB286から診断180秒へ1回だけ更新。GPU延長なし。mainだけがUARTで新boot/restart/Recoveryを確認。

## fresh154到着後に結合する入力

実manifest/原41/native actual/RecoveryID/同Recovery未変更証拠/153結果/最新mutable logsを結合する。Service152停止valuesを含む保持対象をnative actual154から追加し、未取得hashを補完しない。旧152の341記録を154 actualと見なさない。

validator/native clone/post156 reader/正式policy/headerのhashを一致させる。install前baselineの旧boot hashと、update後の新boot hashは分離する。親とbootの片方だけなら書込み前に拒否。ACPI ATCV=1を確認しATCO=1だけ受理、unknown/mixedのguardは緩めない。

署名済parent/FWは再compile/signしない。正式化後のservice1TUだけを既存closureで増分build/signし、main独立sealへ返す。SDK/libs再setupなし。このdraftはservice/native hive/媒体を生成しない。

## source根拠と限界

親thermal_native_parent.incのParentAdcConsumeProofはATCV不一致をSTATUS_REVISION_MISMATCH、ATCO非1をSTATUS_ACCESS_DENIEDで拒否する。FW SharedParentGpuThermal.aslのSHR0 ATCOはSerializedでATCLを一度だけ取得し、親PDO再作成でもboot内状態を維持する。AUXADC単一owner契約を変えない。

main sealのAML sequence [1,1,0,1]はWindows ACPI IOCTLや実温度成功の証拠ではない。新boot起動/復旧・温度はmainの後段実機確認待ち。

## 153後の訂正・343追加の条件

main報告では153はdiagterminal21/finalization0/予定restart1、GPU target10 PB204=0x25（Code37）、PB220なしでclient未実行。Clear到達・描画成功の証拠はない。ModuleIdentity/InitCheckpoint/fresh154 actualは回収待ち。

旧341と342のPE差（COFF0x122/LAAあり/DLL0x140 → COFF0x102/LAAなし/DLL0x8140）はmain調査中の報告であり、この模板からroot原因確定や独立再linkを行わない。

parent-only既定では実actual154の342/Code37を保持して正しく記録し、Started成功へ読み替えない。温度取得とGPU開始失敗は分離して回収する。保持SYS/INF/hash等のimmutable guardは維持する。

GPU343を追加する場合はmainの根拠fix完成・正確な署名tupleの独立seal・一括scope選択が必要。このparent-only draftでは343未選択、artifact/mainsealはnull、GPUclient0。採用時にはDIF mask/order、期待post identity、reader、client/timeout、policy hashを正式scopeとして同時更新する。153のclient未実行をClear結果として扱わない。

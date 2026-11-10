# clock1120を次185へ挿すためのfragment

185 worktreeを変更せず、out専用fragmentとローカル回収照合helperを用意する。実機操作、185媒体生成、GPU360/FW/183固定物、既SDK/cacheへの変更は含まない。既1120親/thermalおよびUMはmain独立seal済みのbytesを再利用し、再build/signしない。

## 挿入時の差分（mainが185を作るときだけ）

`prepare-clock1120-fragment.py --root ROOT --out ROOT/port/xnu/out/新directory`で生成するpackage-map-rows.jsonは、既package()が読むexact parent/thermal rows。新185の現GPU360採用policyへこの2rowだけを置換し、基点のactual baselines/原files/旧service snapshots/registry preservation/他packagesとGPU360/FWの根拠を保持する。183policyを185の基点へコピーしてfresh世代を偽らない。sourceの新fragmentにはGPU packageを含めない。

install_targetsは185既集合にparent/thermalだけ追加。13role順を維持する場合parent index6・thermal index1なのでmask追加は0x42。例えばGPUだけの0x400に対しては0x442だが、他の既採用bitを落とさずORで追加する。stage_orderの13role配置は変えず、install_orderではparent6→thermal1を先行させ、残りの旧relative順を維持する。新owner/root登録やAFE/DIS0/BTなどの追加DIFは行わない。既DIFのDI_DONOTCALLCONFIGMG|DI_NEEDREBOOT、planned restart1、boot fence、署名/hash/metadata guardを保持。

新clientをdiagnostic_clients[0]へ一個挿す。exact initializerはdiagnostic-client-row.inc、JSON rowはdiagnostic-client.json。exeをWindows/System32/mt8167-clock-snapshot.exeへ既署名bytesのままstageし、relative_directory='.'、arguments=['--snapshot']、timeout10000ms、phase0。現在のGPU Presentはphase1で最後のまま維持。他のphase0があれば相対順を保ち、全client数は8以下。

現183のgeneratorはINSTALL=['render']・client count1・GPU index0を固定しているので、そのまま使えない。mainの新185 generatorだけでINSTALLのunionと実client数を反映する。現GPUだけならcount2、GPU index1。A04_DIAGNOSTIC_CLIENT_COUNT、observer/collect-contract/deadline/launch-PID joinのGPU_client_indexを全て1へ揃え、GPU_start_eventはservice診断phaseのPB target1/event0x220とする。client0 PB220をGPU開始や900秒GPU期限延長に使わない。既serviceの180秒等のpre-GPU deadlineを繰返し延長しない。phase0というclient分類とservice全体のinstall/diagnostic phaseを混同しない。

mt8167-peripheral-update.c自体は変更不要。run_clientsはfirstにERROR_NOT_READY等があってもphase0を実行し、phase1最後だけcombined_gpu_node_readyを要求する。phase1拒否はclockのrawを否定しない。service_main→combined_finalizeがDiagnosticのclient0結果を保存する。親/thermal自体が不適合ならUM/queryで厳密拒否し、guardは緩めない。

新service/data_directoryはmainの185 identityへ固定する。事前fresh captureで当該service/data directory・raw・clock stdout/stderrの不在を確認し、生成WIMでもraw/stdout/stderrを事前同梱しない。旧出力の削除・上書きで不在条件を作らない。UM exeのSystem32衝突もabsentまたはexact署名hashの保持条件をmainがpinする。

## rawの所在と回収（mainのみ）

CreateProcessのcwdはexeのSystem32ではなくbase_directory。rawの唯一の取得先はWindows/System32/{新data_directory}/mt8167-clock-snapshot-v1.bin、128B。stdout/stderrは同directoryのlogs/clock_snapshot1120.stdout.txtおよび.stderr.txt。service.logも回収する。directory名を旧182に固定しない。

既main collectorはmanaged directory内のbin/txt/logを回収する。次185用collectorのmanaged_directoryを新identityへbindingし、このrawがmanaged根直下でも回収されることを確認。別fixed項目へWindows/System32/mt8167-clock-snapshot.exeを追加する。親/thermal SYSとSYSTEM3hiveは既fresh186 captureへ含め、原backup/receipt hashを保持。新raw以外の既GPU回収集合は削らない。collection-contract.jsonは差分仕様で、既collector全体の置換ではない。

raw128B、stdout/stderr各4096B、service.log1MiBまで。欠落・partial・timeoutもraw失敗資料として非上書き保存し、成功に分類しない。rawを複数directoryからglobして選ばず、新service exact pathだけを使う。mainが既read-only Recovery経路で回収する工程に差し込み、このworkerのhelperはmount/ADB/再起動をしない。

## 回収後の照合・decode（ローカルのみ）

```
python3 tools/clock/verify-clock1120-capture.py --root ROOT \
 --fixed ROOT/185回収directory/fixed \
 --manifest ROOT/fresh186/manifest.json \
 --receipt ROOT/185reader/receipt.json \
 --policy ROOT/185policy/peripheral-policy.json \
 --fragment ROOT/fragment/release-fragment/fragment.json \
 --out ROOT/port/xnu/out/新clock186照合先
```

mainが新185へ合わせた既native hive readerを使用し、Diagnosticのtyped CombinedResultを含むreceiptを渡す。helperはpolicy token・source manifest・SYSTEM3file hashesをjoinし、client0 observed/phase/id/PID/create/wait/exit/resultを照合。service.logの同RunId/ServicePid segmentでPB target0/event220のClockPidとevent221=0が一組だけ存在することを確認する。親/thermal nodeのmetadata/sys/inf-location/status/Problem、実selected service/HWID/version、actual capture SYS2hashとactual UM exe hashを照合する。

stdoutはCLOCK_SNAPSHOT_RC=0、flags/valid_first/valid_second/countとrawの一致、CPU/DDR known=0を厳密確認。128B/ABI1/count11、unknown markers8/16/32、mask0x7ff両方、単調100ns bracket、all-ones拒否、UNSTABLEと実sweep差分の整合を確認する。default decoderだけを呼びreferenceは渡さない。configured PLL・selected mux親Hz、effective CPU/DDR Hzはnull。sweep不一致はrawの観測として保持しHz成功にはしない。

ObservedResultがGPU不調21でもclockの個別照合を止めない。GPU結果を0へ変更せず、clockが成立してもbatch全体を成功と呼ばない。checked.jsonは証跡の整合性のみ。rawにはRunId/PIDが内包されないため、mainはUARTの同診断起動、唯一のservice directoryと出力事前不在、actual tuple、ClockPid、snapshotのuptime bracketを追加joinしてから今回rawと分類する。helper単独ではsameboot/物理周波数実測を証明しない。

回収前は`raw未取得`のまま。新mapping、clock/rail/PLL/DDR書込み、昇周波はない。連続thermal保護の完成も主張しない。

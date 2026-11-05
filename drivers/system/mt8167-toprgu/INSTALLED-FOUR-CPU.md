# 搭載Windowsにおける4 CPU watchdog引継ぎ

この実装は実験段階であり、搭載OSに導入済みではない。WDKビルドやホスト試験は署名受理・PnP・実機安定性を示さない。CPU1引継ぎ機能は `A04_TOPRGU_CPU1_HANDOFF=1` でビルドした場合だけ有効で、既定値0のSYSと同じファイル名でも動作が異なる。パッケージのバージョンやファイル名だけからビルドオプションを推測しない。

## 既存の観測と差分

- UEFI正本 `1b705d65` の4 CPU WinPEは `CPU1 ownership acquired CPUs>=4`、継続給餌と画面転送を記録。WinPEのESPは `ed92f50a7ba5464226e9781be6613135ed02bada702d3eb23300d85b902bd26e`、TOPRGUとStart=0を事前確認。WinPE固有の署名・起動構成を含む。ログは `port/gsi/logs/ios-feasibility/windows-rt-canonical-1b705d65-run1/observations.md`。
- 搭載Windows側の `windows-rt-canonical-1b705d65-installed-run1/uart.log` は `D0 active CPUs>=4` と多数のkickを示すが、CPU1所有確認マーカーは示さない。別の `windows-rt-toprgu-unlimited-four-{uart,monitor}.log` では通常ESPの4 CPUでD0後660秒間watchdog再起動なしとの監視記録がある。いずれもCPU1確認型が搭載済み、長期安定、電源遷移、今回のSYSの動作を証明しない。実行ファイルのハッシュと搭載OSの現在のサービス・署名状態はこのログだけでは特定できない。
- 旧WinPEの失敗ではTOPRGU未注入WIMでEBS後に開始マーカーがなく次回BOOT_REASON 9。これは搭載Windowsの上記観測と別の試験である。`EARLY-BOOT.md` にWIMとSYSの詳細。

## この変更の条件と限界

4 CPUで論理CPU1上のDPCがMPIDR affinity 1を確認し、watchdog MODEのenableを確認して初めて給餌を開始する。CPU数が4でない状態を観測したら所有権を破棄し、4に戻っても新たなCPU1上のDPC確認まで給餌しない。CPU数の変化が二つの観測の間に完結した場合は検出できない。CPU1がfirmware workerからWindows parkingに移る機種固有契約が前提で、他のCPUやファームウェアによる独立アクセスの排除を証明するものではない。timer kickのUART文字列は実際に給餌レジスタへ書いた時だけ出す。

ハードウェア解放ではrunningを落とし、WDFタイマー停止、backup timer取消し、DPC排出後にMMIOを解除する。電源遷移でwatchdogを止めてからの復帰・CPU1再取得は実機未検証であり、サスペンド／休止／シャットダウンの安全性は保証しない。TOPRGUの停止はファイルシステム書込み中の安全な完了保証ではない。

Windows側はStart=0でもPnPとD0までの時間、署名受理は保証されない。対象の搭載Windowsについて、実際のUEFIとACPI MOU0005資源、現在選択中のControlSet、SYSとCATのハッシュ・署名、BCD署名設定、D0とCPU1取得、経過時間・給餌・次回BOOT_REASONを同一起動単位で照合する必要がある。旧WinPEの証明書を搭載OSが信頼しているとは仮定しない。いずれも記録なしにDISMや置換を始めない。既存 `startnet-upgrade-toprgu.cmd` は固定旧SYS比較とvolume guardがあるが、対象SYSの署名受理やCPU1引継ぎは判定せず、この変更の安全な自動導入を保証しない。

ホスト検証: `python3 tests/toprgu_poll_host_test.py` は実ソースの4 CPU poll関数をMMIO/CPU模擬で実行し、CPU1以外の拒否、確認後給餌、CPU数減少後の再確認、MODE無効、確認途中のCPU数減少を検査する。`make test-toprgu-handoff` は所有ポリシーを検査する。`.github/workflows/wdk81-probe.yml` の `fourcpu_only` は独立したWindows 8.1 WDK ARM32ビルドでSYS/INF/CATとビルド設定・SHA256を成果物化する。出力は未署名で実機投入用ではない。

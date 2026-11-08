# Texture327 suiteの外側時間上限

327のsource/package/EXEは変更しない。これは実測時間の報告ではなく、mainがservice/controllerへ与える保守的な予算。

- serviceが待つclient全体: **900000 ms (900秒)** を推奨。
- normal diagnostic controller phase: **1200秒**を推奨。serviceの900秒に保存・flush・自己停止・収集確認の余裕300秒を加える。
- WinPE prepare、PnP install、予定再起動、Recovery回収、native receiptは別phase。上記をboot全体の上限に流用しない。
- 旧120秒は旧client向け設定であり、新suiteの処理量から正当化された上限ではない。

同一device/contextの12列はClear2 + Triangle1 + Color1 + Texture8。各列800x600のblocking Mapと480000画素のCPU検査、計5760000画素。Triangle/Color/TextureはClear+Draw+Copy、最初のClear2列はClear+Copyなので、APIレベルでClear12/Draw10/Copy12、計34操作。この数をnative job数と同一視しない。

`testclient/d3d-clear.c` の `line` / `text` 系は各行にWriteFile+FlushFileBuffersを行う。shader/pipeline/resource生成、compile、state bind、queue/Map、CPU oracle、NTFS同期書込みをすべて直列に含む。Texture8列には8個のimmutable UV constant buffer作成/解除もある。client内にはGetTickCountによるdeadline、Sleepによる待機、再試行ループは無い。GetTickCountはlog名の識別に使用。

`render_owner/native_owner_factory.c` の `wait_boundary_locked` はKeQueryInterruptTime差20000000(100ns単位、2秒)で特定のowner境界を待つ。これは12列全体、blocking Map、ファイル同期IOの上限を保証しない。`windows_start.c` のpoll_budget20000×poll_delay_us50も単一start境界の設定で、suite上限ではない。

900秒は実測P99/完了保証ではない。最初の実測が得られるまで、12列×45秒=540秒に初期化/終了180秒と余裕180秒を加えた運用予算。45秒/列自体も性能実測ではない。実traceの各phase時間と最終flushをmainが保存して以後調整する。900秒を越えた未完はtimeoutとして記録し、出力欠落を成功に補完しない。

既存native/scheduler/watchdogのtimeout条件やreset処理は変更しない。外側のclient timeoutは停止保証やDMA解放許可を意味しない。実機試験・controller設定はmain/Mencius所有。

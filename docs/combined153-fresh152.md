# run153 / fresh152 GPU342単独検証

fresh152 fee4081f、Recovery72ea1a1d-06b9-4671-88d2-f957198f3362とclean nativepost151を正本にする。151 source c4ce910f/媒体は凍結保持。
DIFはrenderだけ（341→342）。親1116、thermal1.10、MSDC1.10、AFE1034、combo1023、MMA1.10、DOD1.7、LC1.7、BT2013ほかactual SYSを保持する。thermalは151でC00000A2/温度未取得、status/audio exit0とは区別する。主のSHR0 AML修正は混載せずboot1fefを保持。

client9934/--hardware=PowerVR/900000ms/phase1を最後1回。温度/status/audio/WLAN/BT/rotation/benchmarkの機能再試験はしない。保持driverのPnP/選択/署名/bytes資格は検査する。
12target snapshot、render ID/SYS/indexの明示、直前peripheral snapshotとvolume flush、既存SystemRequiredと終端解放、failure-only baseline REPORT、mutable診断prefix観測を保持。

18サービスfull fingerprintsと、実nativeで確認した旧146/BT10 raw values＋停止済み150の5 valuesを保持。未取得の旧150root/subtree hashは作らない。拒否149のservice148不存在を確認する。
通常source closure e2d8d58b、署名recipe e395c3a6、runtime/package59ded879は別出自。GPU342 main seal、manifest d167f2e5、SYS431c06ad、8TU/native67/KMD13/6署名を結合。GPU driverの再compile/link/signはしない。新serviceのみ1TUでビルドする。

install900秒、boot180秒→最初同boot phase1 PB286からpre-GPU180秒→最初GPUclient PB target0/event0x220からGPU900秒を各一度だけ更新。再イベントでは延長せずterminal/crashで早期終了。main observerへの結合をformal collectに固定する。
実機はmainのみ。回収はfresh154。実GPU342成功・温度成功をbuild/clone確認から主張しない。

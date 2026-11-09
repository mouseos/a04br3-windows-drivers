# run95: fresh94 / render3.13→3.14だけの一回限り更新

基点66996e88。ObserveOnlyと4target更新を再利用しない。
専用profile `render-single-gpu314-run95-v1`、compile `A04_RENDER_SINGLE_GPU=1`、
専用service `MT8167RenderSingleGpu`。固定target ACPI\MOU0006だけを配列に持ち、
標準DIF_INSTALLDEVICE/DI_DONOTCALLCONFIGMGで1回更新し、最大1回のplanned reboot後にGPU clientだけを1回起動する。
親1026/Audio1026/MSDC1.8/USB1005/WiFi1012/DOD1.6と既存boot/POST policyを保持する。
既存Engine/音声clientは起動も再armもしない。

fresh94 manifest4ff327d3と明示native七node/hashだけを受理する。
候補はmain正式f4f1b4f9/package314 manifest9ef5d9bf、新native manifestf19d1d1dを専用schemaで読む。
旧linked_manifest形式を当てはめず、package_source/binary_source/kernel_binary_source、baseline/native manifestchainとoutputsを検証する。
SYS adf92be2/FW5c1fdff5をbyte保持、UMDだけ4c0e3ca3に変更する。SYS/FWを再build・再署名しない。
FL10_0限定開発clientのsource/hash/署名manifestを照合し、args--hardware=PowerVR、retry0、timeout120000ms。
standard_profile_qualified=falseのまま記録し、未測定の描画成功を作らない。

新native保持planはfresh94のSYSTEM/LOG1/LOG2と許可済みEngine診断policy履歴を明示する。
準備helperは新service7値だけを登録し、既存disabled Engine7値と全診断leafの型/値/fingerprintを前後・clean reloadで比較する。
66996e88のschema2/count4/clients2、固定Retained0..2HardwareId、MSDC6DWORD、leaf64上限をそのまま維持する。
親/Audio/eMMCの旧baselineへ戻すことは許可しない。

runtimeはrender baselineの選択INF/版/date/class/service/SYSと旧UMDを確認し、
保持6nodeの選択INF/版/date/class/serviceとSYS/FWを前後で照合する。
候補INF/SYS/CAT/FW/UMDの全hashとCAT所属を確認する。
既に3.14なら再installを拒否する。boot fenceとCREATE_NEW markerでinstall boot内のclient実行と複数rebootを防ぐ。
CR37はhealthy DN_STARTED/problem0/!DN_HAS_PROBLEMだけ任意ProblemStatus不在として扱い、CRとnt_valid0を残す。

ビルド: tools/ci/build-ge8300-single-gpu.ps1（既存WDK8.1、service1TUのみ）。
媒体: tools/winpe/build-private-single-gpu-media.py（元WinPE baseを保持。quiet WinPEはこの試験へ混ぜない）。
事後reader: tools/recovery/Read-SingleGpuLastDiagnostic.ps1、関数Read-Mt8167SingleGpuLastDiagnostic。
実機はmain所有。作成・署名・native保持成功と実機描画成功を区別する。

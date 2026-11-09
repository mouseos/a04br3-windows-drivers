# GE8300 343のリンク条件修正

153では342のDriverEntry/Contractが成功した後、DxgkInitializeがC000003Bを返し、Code37になった。ReachedMask7と実GPU342 ModuleIdentityにより、AddDevice以降の旧値は現起動の成功証拠ではない。GPUclientは一度も開始しておらず、Clear修正の成否は未検証。

342で従来のMSVC14.29 linkerをLLDへ変更したところ、COFF0x122→0x102、DLL0x140→0x8140、既.text/.rdata/.data/.pdataのnonpageable属性も失われていた。両SYSのPE checksumは有効だった。これらの差が単独でC000003Bを生むとの断定はしない。343は動作したRT8.1用のMicrosoft14.29リンカーと明示的なLAA/TS属性を戻し、元のsection属性を必須検査する。

sealed342のnative67/KMD13 archiveは全byte同一で再利用し、C/C++/shader/PCO/SDKのcompileを行わない。INFだけ3.43とする。実行コードのbuild identityはGPU342-tq-clear-v1、source59ded879のままであり、3.43はリンク条件の別候補である。新SYS/selectedINF/ModuleIdentity/ReachedMask/実GPUclientを合わせて実機評価する。実機DriverEntry後の正常開始、GPU画素、三角形、長時間安定は未検証。

REAで両SYSのdisplibを比較すると、342のDlpGetServiceNameはUTF16の `\\Registry\\Machine\\System\\CurrentControlSet\\Services\\DXGKrnl` を先頭+1 byteからコピーしていた。343では同じ文字列の先頭からコピーする。343は初期化失敗に直接関係する参照を修正した候補であり、実WindowsのDxgkInitialize成功は別途確認する。ローカル証拠は `port/xnu/out/windows-rt-relink343-rea/function-0x42b080.json` と `function343-0x42b394.json`。これら生ログはソースへ混ぜない。

通常のキャッシュ非依存ビルド `tools/ci/build-ge8300-from-source.ps1` は、既に `drivers/gpu/mt8167/wddm-render/link-kmd-msvc-wdk81.ps1` を使用する。今回の退行は342増分ビルドに渡したhost-tools.jsonのリンク設定変更で発生した。通常ビルドのリンク手順と、固定342 archiveの再リンク救済手順を区別する。

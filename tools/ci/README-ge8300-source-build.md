# GE8300の単一checkout source build

公開repoの一つのcheckoutからnative owner、KMD、PCO/NIR compiler、UMD、限定標準clientをbuildする。
現行はmain320 source f73da1e3のruntimeを統合した開発用source。署名320 package・実機媒体と、ここで生成する未署名成果物を区別する。

## 入力と実行

正規toolchainを備えたWindows x64で、このbranchまたは固定commitをcloneして実行する。

```powershell
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

必要なものはPython3、LLVM clang/llvm-lib/lld-link/llvm-readobj、WDK8.1 ARM、MSVC ARM32/x64、Windows10 SDK UCRT。
ARM32 UCRTのheaderとlibが揃う既存SDKを自動選択する。`-WindowsKit10Version`と`-VcToolsDirectory`で明示も可能。
SDK/CRT/WDKをrepoへ再配布せず、再構築・自動再installもしない。
過去GPU SYS/DLL/lib/obj、C:の既存driver cache、private hive、端末固有入力はbuildに不要。

出力はcheckout内build/ge8300-source。新しいOutputDirectoryを要求し、component manifestとsource/toolchain/hashを残す。
署名、INF/CAT包装、配置、reboot、実機操作は行わない。`-RunHostTests`は実engineをMSVC x64でcompile・実行するhost-only回帰。
spanのreal-MSVC-SEH/WDM modelの独立検証は別receiptとしてプロジェクト成果物領域に保持する。

## source出自と依存境界

- UMD: main3f8291d0。KMDは78ca6565を基点に317 f77f4b07、318646925e1、3192f29acee、320f73da1e3を明示統合。
- 選択MDL span: sidecar21eec1f9のfrontend_memory.c/.h、mdl_pin.c。main319 callerを保持し、MdlOffsetはページ単位で一度だけ適用する。
- native owner: fb217359の54TU/必要source closure。vendor_m1_11/render_ownerへ隔離し、includeの相対参照だけを書き換える。
- 元vendor/DOD/legacyを全面置換しない。source-mapは元commit/hashと可逆なinclude変更を記録する。
- frontend memory ABI v1、共通protocol/native binding、既存他deviceのsource境界を維持する。EDK共通編集なし。

geo_frontend_memoryのselected view identityと、独立probe済みfull pinned MDLは別物。
元WDDM MDLをunlock/freeせず、map/native leaseが消えた後だけ自分のviewとpinを解放する。
319のCacheCoherent要求は既存の実GPU/CPU cache-maintenanceを前提とし、hardware coherent segment capを広告しない。

## 320のprivate DMA契約

実RT FUN5079cで、Render返却private pointerと元baseの差分をsubmissionのprivate used bytesへ渡す経路をmainがREA確認した。
317でdocs[in]からprivate cursorを固定した判断は撤回済み。
両Render pathでDMA/privateのticketを書いた後、両cursorをsizeof(ticket)=16進める。capacity値は入力のまま保持する。
R28/R29は実range/flags/contextのcheckpointで、status0自体をPatch成功と扱わない。
source回帰testの誤った「private pointer不変」assertも320正本へ訂正した。
詳細はdocs/ge8300-private-cursor320.md。

## compiler source

third_party/mesa-pcoはMesa0d94b3f3由来の実消費sourceとtarget中立生成C/header、Windows ARM portのsnapshot。
370TUをsource compileし、旧reference/link manifestやprebuilt compiler archiveを入力にしない。
生成Cがあるため別machineのMeson cacheを要求しない。source-planへupstream/port/license/hashを記録する。
NIR生成器のfloat16_tローカル別名を改名してACLE headerとの衝突を回避し、元生成byteへ逆変換できることを確認した。
pvr_device_info TUだけ旧stricmp名を同契約の_stricmpへ対応する。
GNU privateIR packed bitfield/C ABIをWindows DDIのMSVC ABIへ混入させない。対応範囲は既存限定FL10 profileのまま。

## CIと検証範囲

.github/workflows/ge8300-source-build.ymlはworkflow_dispatchのみ、contents:readだけ。
正規toolchainを備えたself-hosted [Windows,X64,wdk81-arm32]で同じwrapperを実行し、未署名SYS/DLL/client、component manifests、PE metadata、host receipt/stdoutを回収する。
署名秘密鍵、SDK installer、実機操作をCIへ入れない。GitHub workflowを実dispatchしたかは個別receiptで区別する。

初回5cf3898e、317ba8ed4b6、3186bee618aのfull build成果物は別出力として保持した。
319/320署名候補のincremental buildと、この公開fullsourceからのbuildはsource/manifestを別に記録する。
ビルド成功・署名成功・実GPU描画成功は別。source-build-manifestのhardware_verifiedはfalseを維持する。

318646925e1からの既報payload slot競合（Render冒頭回収後のIRQ完了→同slot再予約→旧draw pointer上書き）は、320 cursor修正では未修正。
host engine回帰はこのKMD競合まで検査していない。主担当の訂正commitを後続で明示統合する。
一般D3D/長期安定を資格済みとせず、この出力を現署名媒体へ自動適用しない。

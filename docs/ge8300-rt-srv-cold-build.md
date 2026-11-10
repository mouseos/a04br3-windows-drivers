# RT/SRV fixed source cold buildとPCO375完全cache回収

RT/SRV候補のWindows buildをmainが既VM42796448681で実行する手順。
current367、サービス196、run197のcheckout/出力を変更せず、別checkoutと新しい出力directoryを使う。
本手順の作成時点ではrunner・実機・REAを操作していない。RT/SRVの実hardware/DWM動作は未確認。

## 固定sourceから一度だけcold build

公開branchはfeat/ge8300-rt-srv-transitions。
HANDOFFで通知した公開HEADをExpectedSourceCommitへ渡す。以下のCOMMITはその40桁値に置き換える。
既設のVC14.44.35207、VC14.29.30133、WDK8.1 headers/KM libraries、UCRT22621、LLVM/Pythonが必要。
8.1 conventional UM lookupはmainが復元済みの10.0.17763.0 ARMへのjunctionを使用する。
helperはSDK/toolchainのinstall・build・junction作成を行わない。
必要なUM/KM/API library lookupが欠ける場合は375 compile前に停止する。

```powershell
$commit = 'COMMIT'
git clone --single-branch --branch feat/ge8300-rt-srv-transitions https://github.com/mouseos/a04br3-windows-drivers.git C:/mt8167-rt-srv-cold198
Set-Location C:/mt8167-rt-srv-cold198
git checkout --detach $commit
$out = "$PWD/build/rt-srv-cold375"
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ci/build-ge8300-rt-srv-cold.ps1 -EnableCandidate -ExpectedSourceCommit $commit -OutputDirectory "$out" -VcToolsDirectory "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207" -KernelVcToolsDirectory "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133" -WindowsKit10Version 10.0.22621.0 -CompilerJobs 2
```

build-ge8300-rt-srv-cold.ps1はVerifiedCompilerDirectoryを公開せず、fresh outputに一度cold buildする入口。
PCO375（pair込み）をsourceから実compileした後、native/KMDより先に完全compiler archiveを作る。
その後、native67・KMDの全依存TU・表示C++・mdl_pin・UMD/clientを同一fixed HEADからfresh compile/link。
表示10TU＋mdl_pinはcl14.29、両SYSはSHA固定LINK14.29。
PCOのAPI/CRT/include入力とnative shader codec引数のVCは14.44のまま。
source-build-manifestはnative/KMD/UMD各子manifestのsource_commit/cleanを検査し、
PCO/kernel両VC directory、kernel compiler/linker SHA、両SYS link metadata、cache archive/receipt SHAを記録する。
nativeの既存clang compile方式は維持する。SDK buildは0。

全compile sourceはExpectedSourceCommit。driver/header変更前の367/aa661 objectやUMDを混在させない。
旧f4のmetadata、359の6/375一致objectからcompilerを復元したとは扱わない。
新たに生成したcompilerは実compile時点のsource commitを記録する。

## 最優先回収物：driver build完了を待たない

compiler compileが完了すると、以下が先に生成される。

- C:/mt8167-rt-srv-cold198/build/rt-srv-cold375/compiler375-cache.zip
- C:/mt8167-rt-srv-cold198/build/rt-srv-cold375/compiler375-cache.zip.receipt.json

この2ファイルを既存の回収手段で、まずlocalの指定root内へ回収する。
推奨local名はport/xnu/out/windows-rt-rt-srv-cold198-compiler375.zipと同名.receipt.json。
後続のnative/KMD/client build失敗を理由にcompiler375を再compileしない。
compiler directory自体も既VMで保持し、archive回収・local照合完了前に削除しない。

archive内のcompiler/には全375 object（別途pco_pair.objも保持）、10実library
（source-built4＋Microsoft API/CRT6）、compile/link/pair manifest、cache-evidence.json、
375個の生.d、375個のcompile log、VFS overlay、その他compiler directory内の全実ファイルを保存する。
recovery/には固定source-planとcompiler build/strict verifier recipeを保存する。
compiler-cache-inventory.jsonは全member SHAを保持する。

object/library/生dependency/compile-time include証拠が1つでも欠ける場合はarchive成功としない。
一時.partialへ作成し、ZIP内全memberのCRC/SHAと元cacheの不変性を検証後、完成名へatomic renameする。
receiptにはZIP全体SHAと個数・compile source commitを記録する。
単なるmetadataだけの回収を完全cache回収と扱わない。

```powershell
$receipt = Get-Content "$out/compiler375-cache.zip.receipt.json" -Raw | ConvertFrom-Json
if ((Get-FileHash "$out/compiler375-cache.zip" -Algorithm SHA256).Hash.ToLowerInvariant() -cne $receipt.archive_sha256) { throw 'Cache ZIP SHA mismatch' }
python -B tools/ci/archive-ge8300-compiler-cache.py --verify-archive "$out/compiler375-cache.zip"
```

local回収後も、receiptのarchive_sha256との一致確認と--verify-archiveを行う。
このverifyはWindows/SDK/compilerなしでも実行でき、compileやinclude probeを起動しない。
archive復元後はExpand-Archiveの展開先/compilerが元cacheの全内容になる。
source commit、古いcommand内のパス、過去のdependency traceを書き換えない。

archiveは外部SDK headers/tool binary自体を同梱しない。
全include証拠は実compileの-MD（system include込み）の記録とSHA・生依存リストであり、
archive検証だけで新しい環境の全include解決を認証したとは扱わない。
既設/保存済みのlicensed SDK/toolchainを維持し、再利用時に既存strict verifierで現在のtool/API/全includeを照合する。

## compiler完了後にarchiveだけ再作成する場合

driver失敗やarchive先の問題に対する復旧であり、compilerは再compileしない。
必ず同じfixed sourceと既存compiler directoryを使い、新しいarchive名へ出力する。

```powershell
$linkPin = (Get-FileHash "$out/compiler/link-manifest.json" -Algorithm SHA256).Hash
python -B tools/ci/archive-ge8300-compiler-cache.py --cache "$out/compiler" --expected-link-sha256 $linkPin --output "$PWD/build/compiler375-recovered.zip"
```

## driver build完了後の回収

通常の完了印はGE8300_SINGLE_CHECKOUT_SOURCE_BUILD_COMPLETE。
次も回収する。compiler cache ZIPの回収をこの完了まで遅らせない。

```powershell
Compress-Archive -Path "$out/native","$out/modules","$out/umd","$out/*.sys","$out/*.json","$out/*.map" -DestinationPath "$PWD/build/rt-srv-cold198-driver-products.zip"
Get-FileHash "$PWD/build/rt-srv-cold198-driver-products.zip" -Algorithm SHA256
```

local推奨名はport/xnu/out/windows-rt-rt-srv-cold198-driver-products.zip。
source-build-manifest.json、source-origins.json、kernel-link.json、transport-link.json、
両SYS/maps、UMD DLL/client、native/modules/umdの実object/archive/子manifestを保持する。
失敗した場合も既生成のobject/archive/logを維持し、module metadataが未生成ならその事実を記録する。
署名/CAT作成、サービスprofile、媒体作成、実機試験はこのrecipeの範囲外。

## 次回以後：同じcacheをstrict再利用

fullsource wrapperの既存CLIは維持する。既存引数の順序・cold/strict選択は変更せず、
KernelVcToolsDirectoryとArchiveCompilerCacheを末尾に追加した。
KernelVcToolsDirectory省略時は従来どおりVcToolsDirectoryをkernelにも使う。
今回のRT/SRV cold入口では14.29を必ず明示する。

同じ環境/固定sourceでcacheを再利用する場合の例：

```powershell
$linkPin = (Get-FileHash "$out/compiler/link-manifest.json" -Algorithm SHA256).Hash
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -UnifiedDisplay -VcToolsDirectory "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207" -KernelVcToolsDirectory "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133" -WindowsKit10Version 10.0.22621.0 -VerifiedCompilerDirectory "$out/compiler" -ExpectedCompilerCacheManifestSha256 $linkPin -OutputDirectory "$PWD/build/rt-srv-next-with-strict-cache"
```

これは次回のdriver buildが必要な場合の入口であり、今回のdriver失敗時に全driverを反復する指示ではない。
source/全include/args/tool/APIが一致しなければ停止し、PCO/SDK再buildへ自動fallbackしない。
build-ge8300-compiler-source.pyとge8300_compiler_cache.pyの既存strict証拠形式・検証条件は変更していない。

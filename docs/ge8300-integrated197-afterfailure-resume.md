# integrated197 のclient link失敗後の再開

native67とmodules全object/archiveは実compile済み。固定sourceは
3d4a3d1097b7f6a796972e0425e37d0263b2b070。
module build-manifestはclient linkより後に書くrecipeだったため存在しない。
既設8.1 UM library lookup失敗に対し、compileを反復しないlink-only helperを追加する。
既存cold recipeは変更しない。

実回収先 port/xnu/out/windows-rt-integrated197-pre-resume の
pre-resume-inventory.jsonと全115入力のSHAを公開specへ固定した。
467個の固定3dソース・recipe入力も照合する。既存KMD archive24 member、
UMD module archive9 memberがそれぞれ回収済みobjectとbyte一致することを確認した。
native manifestはsource3d・clean・67TU。全記録output hashを照合する。
旧aa661 UMD receipt/DLLも照合する。実機試験はしていない。

## mainが実行するコマンド

helperとpinsを含む公開branchをfetch/pullしたcheckoutから実行する。
source-rootは既存3d checkoutのままでもよい。既存driverソースや旧recipeを編集しない。
helperを含む新HEADへ更新しても467入力が同一なら使用可能。
出力は既存inputとは別の未使用directoryへ保存し、旧object/archive/logを変更しない。
UM library指定はmainが復元済みと報告した既設10.0.17763.0 ARMを明示する。
KMは既設8.1への復元済みjunctionを使用する。helperはjunctionやSDKを作らない。

```powershell
Set-Location C:/mt8167-shared-integrated197
git fetch origin integrate/shared-surface-main194-sharedprimary
git merge --ff-only origin/integrate/shared-surface-main194-sharedprimary
python -B tools/ci/resume-ge8300-integrated197-afterfailure.py --enable-candidate --validate-only --source-root C:/mt8167-shared-integrated197 --build-directory C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd --output C:/mt8167-shared-integrated197/build/integrated197-link-resume
python -B tools/ci/resume-ge8300-integrated197-afterfailure.py --enable-candidate --acknowledge-reconstructed-afterfailure --source-root C:/mt8167-shared-integrated197 --build-directory C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd --output C:/mt8167-shared-integrated197/build/integrated197-link-resume --um-arm-libraries "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm" --kernel-arm-libraries "C:/Program Files (x86)/Windows Kits/8.1/Lib/winv6.3/km/arm" --microsoft-linker "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe"
```

別checkoutにhelperを置いた場合はpythonのscriptパスだけそのcheckoutの絶対パスにする。
validate-onlyはsource/pins/archive検証だけでありcompile/linkを実行しない。
Windows実行時はinputがsource-root内、outputがsource-root内の新しい別directoryであることを要求する。

1. 全実入力SHA、固定source、native manifest、archive-member/object一致を検査。
2. 既存objectからD3D11/D3D9 clientをlinkしARMNTを検査。
3. modules/build-manifest.jsonを新output側へ生成。旧archivesをそのままcopyする。
4. 既存transport356/scanout objectをarchiveし、両SYSをSHA固定LINK14.29でlink。
5. 各link metadataとintegrated-build-manifest.jsonを生成。

native/KMD/UMD compile、PCO375 compile、SDK buildはゼロ。
LINK14.29とdisplibは既存固定SHAを要求する。新たに用いたUM/KM libraryとLLVM toolのhashは
link時点のlive hashとして記録し、処理終了前に再照合する。
compile時に使った外部SDK/toolのhashだとは扱わない。

## metadataの証拠範囲

source_commit/compiled_source_commitは3dのまま。recipe_source_commitはhelper側HEAD。
module_metadata_reconstructed_afterfailure=true、recipe_derived_flags_only=true、
historical_compile_arguments_recorded=false、module_historical_include_trace_recorded=false。
固定recipeのcopy/hashと元build.log hash、保存object/archive hashを保持する。
compile時のMSVC実toolを独立に再照合した証拠はないため、その項目はfalse、
cl14.29 hashは固定recipeが要求するexpected値として記録する。
不足している旧include traceや実command traceをcurrent値で埋めない。

この復旧処理はmainの実行記録・申告と固定recipe/実保存物を根拠にmodule metadataを再構成する。
compiler生成の完全なcompile/include証明にはしない。
helperは失敗時に停止し、compile fallbackを持たない。
再実行する場合は部分出力を完了扱いせず、新しいoutputを使う。

公開時点の検証はLinuxでの実回収input検証まで。Windows client/SYS link成功、署名、
実機、DWM成功は未確認。RT/SRV兼用・blendは未実装のまま。

## mainが先に生成した両SYSを再linkしない場合

mainの既存SYS link完了後、同じhelperに以下を追加する。
このmodeではLINK/llvm-libを起動しない。client linkとmetadata生成のみ行う。

```powershell
python -B tools/ci/resume-ge8300-integrated197-afterfailure.py --enable-candidate --acknowledge-reconstructed-afterfailure --source-root C:/mt8167-shared-integrated197 --build-directory C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd --output C:/mt8167-shared-integrated197/build/integrated197-metadata-resume --um-arm-libraries "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm" --existing-sys-directory C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd --existing-link-record C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd/main-link-resume.json --acknowledge-existing-fixed-input-links
```

existing-sys-directory内にge8300-render.sys、mt8167-display-transport.sys、
kernel-link.json、transport-link.json、ge8300-render.map、transport.mapが必要。
mainが別名で保存した場合はその実ファイルを同名へcopyして用意する。
main-link-resume.jsonは元のpending/wholewrapperfalseを保持してcopyし、
一括manifestにも元記録として残す。mainのrecord実保存先が違う場合は引数で指定する。
mainbuildloglink-existing-host.logが同directoryにあればhash/copyする。

両SYSはPE ARMNT、output SHA、metadataに記録されたLINK14.29 SHAとdisplib SHA、
map実体を検査し、処理完了時に元hashを再照合する。
元link-kmd scriptのmetadataにはinput archiveのhash/argumentsがないため、
そのmetadataだけで入力archiveまで独立に証明したとは主張しない。
existing_link_input_archives_independently_bound_by_link_metadata=falseを記録する。
固定3dのinput115個は別途照合し、既存SYSがその固定入力から生成された点は
mainの記録と明示acknowledgementを証拠として保持する。
SYS_relink_invocations=0、全TU再compile=0。

### main exec19436の実生成SYSを固定

回収済みwindows-rt-integrated197-linked.zipを一次入力として、両SYS・両map・
両link metadata・main-link-resume.jsonの実SHAをspecへ固定した。
既SYSmodeではこの固定SHAにも完全一致を要求する。

- ge8300-render.sys: 482f4325c004b43d19300adff49ee22680aabc06f97022dd91803d4b06535099
- mt8167-display-transport.sys: 9cd6e6d817f07b126955e16f8cc4bce178364f9f5ec7a88914db0e57bab03bc4

main記録はcompile_source=3d、objects_recompiled/PCO_rebuilt/SDK_rebuilt=0、
signed=falseを確認する。module_full_metadata_pending=trueと
full_wrapper_invocation_complete=falseは過去のmain記録として変更せず保存する。
helperが生成する復旧後metadataとは区別する。
実回収SYSを用いたvalidate-onlyは成功。SYS/client link実行はこの作業では行っていない。
次にmainが実行するのは上記「両SYSを再linkしない場合」のコマンドのみ。

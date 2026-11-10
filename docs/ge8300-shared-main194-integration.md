# shared owner・POST366・SYSTEM194・shared-primary 統合

統合元: main 45d2b3f1332b87a5906d41ee9a21d4c16d773cf9、shared
79b053f9321a328a6cdc45e64a9d4ea811703466、Herschel PR1
bbc26bbd6de9dcb23678c70cd623ce201e2ab783。コード統合コミットは
ddb4b5cd443fc9c802f86b286a03f0d1c151f647。

main のサービス194・POST366観測・checkpoint共有headerを維持。
miniport は共有資源が残るRemoveを拒否する。kmd_objects はv3 descriptor照合、
既存owner GetHandleData、device別open_count、未完了jobのclose拒否を維持する。
shared-primary のサイズ欄を入力capacityと扱わないHerschel修正を統合した。
v2 primaryとv3一般共有resourceを区別する。
originにはmain/shared双方の旧行を入れ子の履歴ごと保存し、verifierで固定する。
native67TU routingと47 overlayはmainの内容を維持する。

## 回収済みaa661 UMDの再利用監査

UMD manifestの27 compile source、再帰的なrepository内literal includeを含む60ファイル、
UMD build recipeがaa661とbyte一致。条件分岐内のincludeも含め、macro includeはなかった。
main以降の変更はこのUMD closureに入らない。回収済みDLLのSHAもmanifestと一致した。

- UMDSourceCommit: aa6613149e09b7b7be718c90103a34323c362917
- DLL SHA256: ebba6b7af5683ae149cfa864578c252d7a7c6de0a591ee5fe470c284bc936bda
- CompilerSourceCommit: f4b92404c4bbe139524a997c5427a8245560e522
- 未署名、runtime/DWM未確認、limited development profileのまま。

compile時の外部SDK/tool include全closureと全実argsはUMD manifestに未記録であり、
新strict cache適合や完全なcompile-time include証明とは主張しない。
現recipeのargs生成は同一。旧binaryを再compile/relinkせずSHA固定のtool成果物として使うため、
現在環境のPCO library復元はこのbinary再利用の必須条件ではない。
原manifestを保持し、新KMDのsourceとしてUMDを書き換えて記録しない。

## 次のWindows kernel候補build

mainが新checkoutで実行する。今回runner・実機は操作していない。
Warm196Directoryは回収済みのge8300-umd.dllとumd/build-manifest.jsonを含む実在rootに指定する。
以下のC:/mt8167-shared-surface-warm196-productsは復元先の例であり実在を未確認。
出力は未使用のcheckout内パスにする。

```powershell
git clone --single-branch --branch integrate/shared-surface-main194-sharedprimary https://github.com/mouseos/a04br3-windows-drivers.git C:/mt8167-shared-integrated197
Set-Location C:/mt8167-shared-integrated197
git rev-parse HEAD
python -B tools/ci/verify-ge8300-fullsource.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/ci/build-ge8300-integrated-kernel-fixed-umd.ps1 -EnableCandidate -EnableFixedAa661Umd -UnifiedDisplay -Warm196Directory C:/mt8167-shared-surface-warm196-products -KernelVcToolsDirectory "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133" -OutputDirectory C:/mt8167-shared-integrated197/build/integrated-kernel-aa661-umd
```

native67・KMD全体を現在HEADでfresh compileする。表示10TUとmdl_pinはcl14.29、
両SYSはLINK14.29にSHA固定。既存nativeのclang方式は維持する。
PCO375・SDK・UMDを再buildしない。PCO library入力も不要。
VC14.44/UCRT22621はnative shader codecの既存引数生成等に使う既設header設定であり、
PCO再compileの指示ではない。GPU366のUCRT26100と完全同一環境とは主張しない。

integrated-build-manifest.jsonは新native/KMD・両link manifest・旧UMD receipt・
origin/map・出力SHAを一括記録する。UMD sourceはaa661、新kernel sourceはcurrent HEAD。
サービス194の適用・署名・実機投入はrecipeの範囲外。

warm196の11TU resumeはこの統合sourceで明示拒否する。
旧driver objects/archiveの部分流用をしない。
PCO full libraryが復元できた場合の全fresh warm recipeも維持するが、
その復元を次のkernel候補化の必須条件にはしない。cold/strictの既定は不変。

## 検証と残存制約

source-only fullsource verifier、origin改変拒否、共有UMD/KMD host検証、
shared-primary size/fill契約検証を実施。hostはOS/nativeをmockし、実GPUを検証しない。
共有resourceはSHARED DEFAULT Texture2D single mip/slice/sample、
SRV-only BGRA/RGBA または RT-only BGRA。既存native storage制約を維持。
RT/SRV兼用・blendは未実装で別範囲。Windows build・署名・実機/DWMは未確認。

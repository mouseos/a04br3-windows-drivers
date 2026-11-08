# 現行327 source build

公開branchは`work/ge8300-texture-filter327`。ルートREADMEのshallow clone手順から
通常`build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests`を使う。
326 texture overlay＋327 regular filter＋独立standard clientまでsource gateへ固定済み。
native58/compiler372TUを全sourceからbuildする経路と、既存依存からの増分署名は区別する。
現hostは増分検証のみで、通常fullbuild/CI全実行済みとは主張しない。
以下は以前の手順の履歴として保持する。

> 3.25追記: work/ge8300-single-checkout325 はnative322+viewport325、KMD320、UMDshader323+bounded input witnessを保持。
> origins/map/validatorは、元native出自を残した固定2file overlayだけを許可する。324 laneは含まない。
> source buildは旧cache無し。既存署名package325のbyte再現・実機成功は未主張。以下の323手順を同branchで使う。

# GE8300 3.25の単一checkout source build

公開repoの一つのsource treeからnative322、KMD320、PCO/NIR、UMD shader323、固定clientをbuildする。旧runnerのC:cache、旧GPU SYS/DLL/obj/lib、private hive、別branchのcheckout、過去commit履歴は入力にしない。失われたarchiveを探索・再構成する増分helperは、通常の公開build入口ではない。

## 通常の公開手順

既存の正規toolchainを備えたWindows x64で次を実行する。

```powershell
git clone --depth 1 --single-branch --branch work/ge8300-single-checkout325 https://github.com/mouseos/a04br3-windows-drivers.git
Set-Location a04br3-windows-drivers
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate -RunHostTests
```

必要なのはPython3.11以降、LLVM clang/llvm-lib/lld-link/llvm-readobj、WDK8.1 ARM、MSVC ARM32/x64、ARM32 UCRT header/libを備えたWindows SDK。使用する既存SDKは自動選択し、`-VcToolsDirectory` / `-WindowsKit10Version`で明示も可能。SDK installer、SDK再構築、署名秘密鍵、実機、端末固有データは必要ない。

出力はcheckout内`build/ge8300-source`。既存出力は上書きせず、新しい`-OutputDirectory`で再実行する。defaultのfullbuildは毎回sourceから全componentをcompileするので、前回output/cacheの存在を前提にしない。source/compiler/WDK/toolchain/最終outputsのhashと出自をmanifestに残す。署名、INF/CAT包装、配置、rebootは行わない。

source配置の確認だけなら次を使う。これはcompile成功を意味しない。

```powershell
python -B tools/ci/verify-ge8300-fullsource.py
```

## 実コードの出自と境界

|component|実装の基点|
|---|---|
|KMD|320 `f73da1e31755e08727958ac6f9160a0d5ccdff6b`。private DMA cursor16/capacity入力保持、Render-before-residency、selected MDL spanを保持|
|native owner|322 `f96948533601f9fdcdcc22b9700aacd96dbf7204`。321のR34..39境界を継承し、必須reset anchor `0x5a8000000`を検証。family writeALL/preidle/FWoff/IRQclose/parentOffNoDMAは維持|
|UMD base|実署名b408のsource `3f8291d094c01206dfaa0a22d1fdccd6eecc5823`。tokenized以外のtyped traceも保持|
|shader wrapper|323 `eb4dca0ee3134c9a9baea4021325f4a924872444`。RTが初期化しないRegisterComponentTypeを読み取らず、MinPrecisionと実float token decoderの制限を保持|
|viewport / input witness|325 `1e4ca5d11701a809653c518e07b75aa6c8cfb409`。native PPP a0/m0 order修正とUMD初回flat入力記録。元native322のhashは保持し固定2file overlayを明示。324 compiler laneは未採用|
|固定client|`f4f1b4f94ade6f59efef042f680c2c3e363442f8`のsourceをcompile。実署名f3afとのbyte同一を意味しない|

nativeは`vendor_m1_11/render_owner`の必要113fileだけを固定native322から取り込み、相対includeを補正し、325でviewport2fileの明示overlayを追加した。package側treeに残る旧vendor実装をnative322の正本と誤認しない。元vendor/DOD114fileは保持し、全面置換しない。共有parent実装/EDK共通の変更はない。

`ge8300-native-source-map.json`がnative元commit/hash/include補正を記録し、`ge8300-fullsource-origins.json`がKMD/UMD/client/legacyのsource hashを固定する。source verifierはGit履歴を読まず、shallow checkout内だけで全sourceとcompiler snapshot597fileを照合する。native54TUのmap自体も固定し、scope内sourceが正しくてもcompile先だけ旧vendorへ戻す誤結合やreset helper欠落を拒否する。Windows checkoutの改行は.gitattributesで固定する。

`third_party/mesa-pco`はMesa `0d94b3f397fb9cf8470bb5600ada8af79ff5361a`由来の消費source、target中立生成C/header、Windows ARM portのsnapshot。370TUをsource compileし、prebuilt compiler archiveを入力にしない。生成済みC/headerがあるので別machineのMeson cacheは不要。license、port patch、元hashはsource-planに保持する。旧fullsource320とcompiler snapshotは同byteである。

## 回帰とCI

`-RunHostTests`は隔離source fixtureでの旧vendor誤結合/単位欠落/別TU alias拒否と325 overlay欠落/旧origin誤帰属拒否、actual clear engine回帰、production viewport325の4096矩形/119頂点projection回帰と、実WDK構造体/production tokenized wrapper/SM4 decoder回帰を実行する。旧wrapperの未設定type拒否を再現し、修正版では00/A5/FFのtype storageでも固定client VS/PSを受理、MinPrecision/unsupported opcodeを拒否する。旧source fixtureはtree内に固定し、過去commitへgit-showしない。PCO code生成はこのwrapper回帰ではstub化されるが、driver DLLには本物のsource-built PCO/pairがlinkされる。

`.github/workflows/ge8300-source-build.yml`はworkflow_dispatchのみ、contents:read、self-hosted `[Windows,X64,wdk81-arm32]`で同じ公開wrapperを使う。checkoutはdepth1。SDKのinstall、private input、署名、実機操作はしない。SYS/DLL/clientとcomponent manifests/host receipt/source originsを回収する。

## 検証と実機の区別

旧fullsource320 `e5816a86`は新規公開cloneからnative54/module24/compiler370/UMD-client21TUをsource compile/linkして回収済み。本323 treeはその手順を再利用したsource候補で、既存署名package323と同一binaryという主張ではない。新版の検証範囲は引渡しHANDOFFに記録する。GitHub workflowのdispatchや本treeのfullbuildを未実施なら、旧320の成功を新版の実行成功に読み替えない。

実3.22では主担当から標準D3Dの2色連続Clear/readback各480000画素一致・不一致0・bugcheck無しが返却され、その次のTRIANGLE_VS作成が失敗した。3.23 shader修正を含むこの公開fullsource buildは実機未検証。一般shader/三角形/UI全描画・全device・長期安定の完成ではない。

## 独立compiler後続328

通常source recipeは[SM4即値差分328](../../docs/ge8300-sm4-literals328.md)を含む。native/UMDの327 texture契約を保持。通常fullsource gateのsm4出自/hashも同時更新し、cache専用差分だけを再現条件にしない。一般shaderのnative受理は未実装であり、compiler候補DLLを媒体へ自動混載しない。

327 suiteのservice/client時間予算は[専用説明](../../docs/ge8300-texture-suite-time-budget.md)を参照。

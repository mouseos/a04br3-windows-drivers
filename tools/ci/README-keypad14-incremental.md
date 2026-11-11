# Keypad 1.4 観測候補のunsigned増分build

基点a13bf474の7first-only入力境界CPをそのまま保持し、INFだけ0.0.1.4/11-11へ識別する。入力動作修正ではない。実signed13 manifest c2a7c929/source3414とretained12 manifest/2objを固定する。state/pipelineのsourceとrepository headers、原codegen/obj hashを確認し、2objは実13で保持されたbyteをそのまま使用する。原Linux18のresource header/hashや環境は未記録であり、新たに完全証明したとはしない。

`mt8167-keypad14-inputs.json`で小ZIP・baseline manifests・現source/header/INF・tool3hashをpin。小ZIPはWDF1.13の42headers/2libs、retained2obj、baseline manifestsと全header indexだけ（約498KB）。既gdi376 WDK2476headers/4libsは同じhashをその場で読み、SDK/全headersを再コピー/再構築しない。PFX/identity/署名toolは含めない。

新taskの固定source cloneで次を実行する。

```
python -B tools/ci/build-mt8167-keypad14-incremental.py --expected-source-commit FIXED_SHA --inputs-zip SMALL_ZIP --wdk-root EXISTING_GDI376_WDK --output SOURCE/build/keypad14
```

現clang20/MSVC14.29 linker/llvm-readobjは実hashで確認する。新wdf_keys.c 1TUだけcompileし、元2objと実WDK/WDF6libsでMSVC link1回。CCC/include環境を拒否し、新compileは明示no-default-config、MDで実依存を記録する。source/headerをprehashし実depの一致をcompile後・link後に確認する。compile済obj/compile-manifest/state/logを先に保存し、後段失敗時も削除や再compileをしない。失敗後の全recipe再実行は禁止し、保存された成功境界から個別に続行する。

成果物はunsigned SYS/INF、3obj、全実dep/全header index、tool/lib/source/argv/outputs hashとmap/log。CAT/署名/配備/押下試験は行わない。既a13 source review/ARM fixtureを再利用し、追加host fixture列やGPU buildを起動しない。候補versionを付けても入力動作成功や配備可署名済みを意味しない。

## 実初回compile後のlink再開

source39966436の実1TUはARMNT compile0で成功した。MDにはVC14.44のstdint.hが1件追加で現れ、事前indexから漏れていたためlink前に停止した。object/hashとMD/state/全argvを保全している。修正版はこのheaderを明示pinし、通常の将来compileでは事前照合する。

今回だけの`--resume-from OLD_BUILD --native-admission-receipt SAME_RUN_NATIVE_RECEIPT`は、成功objの固定SHA/compiled source39966436/原argv/原source/header/INF/原prehashを厳密照合し、compileを呼ばずlinkへ進む。追加stdintは元compileのrecipe内ではprehashされていないことを保持し、同runnerで先に受理されたnative input snapshot cb4442d1のVC全root一致receiptと実stdintの固定hashで補強する。過去の未記録値を捏造しない。新unsigned manifestはcompiled source39966436と修正版recipe sourceを別々に保存する。保持成功objを捨てず、2回目compileをしない。

## 回収metadataの正規化

実1TU compile/source39966436、保存objを使うMSVC14.29 link/recipe e2eb21f5は成功。回収manifestのtarget/相対pathのバックスラッシュ表記不備だけを修正する。`finalize-mt8167-keypad14-unsigned.py`は実raw manifest SHA/実SYS・INF・3obj・88dep・現runtime source/headerを検証し、別outputへcanonical manifestとbyte不変SYS/INFをまとめる。原manifestはraw名でbyte保存し、compiled/recipe sourceを現metadata sourceへ偽置換しない。再compile/link/signは0。通常build recipeの将来出力も同じcanonical表記へ直す。

既sign-mt8167-keypad.ps1は1.2/1.3だけを受理するため、そのまま1.4の署名には使えない。署名許可後に1.4 schema/provenanceの厳密gateを別途reviewする必要がある。今回のunsigned候補では署名guardを緩和しない。

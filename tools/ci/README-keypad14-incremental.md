# Keypad 1.4 観測候補のunsigned増分build

基点a13bf474の7first-only入力境界CPをそのまま保持し、INFだけ0.0.1.4/11-11へ識別する。入力動作修正ではない。実signed13 manifest c2a7c929/source3414とretained12 manifest/2objを固定する。state/pipelineのsourceとrepository headers、原codegen/obj hashを確認し、2objは実13で保持されたbyteをそのまま使用する。原Linux18のresource header/hashや環境は未記録であり、新たに完全証明したとはしない。

`mt8167-keypad14-inputs.json`で小ZIP・baseline manifests・現source/header/INF・tool3hashをpin。小ZIPはWDF1.13の42headers/2libs、retained2obj、baseline manifestsと全header indexだけ（約498KB）。既gdi376 WDK2476headers/4libsは同じhashをその場で読み、SDK/全headersを再コピー/再構築しない。PFX/identity/署名toolは含めない。

新taskの固定source cloneで次を実行する。

```
python -B tools/ci/build-mt8167-keypad14-incremental.py --expected-source-commit FIXED_SHA --inputs-zip SMALL_ZIP --wdk-root EXISTING_GDI376_WDK --output SOURCE/build/keypad14
```

現clang20/MSVC14.29 linker/llvm-readobjは実hashで確認する。新wdf_keys.c 1TUだけcompileし、元2objと実WDK/WDF6libsでMSVC link1回。CCC/include環境を拒否し、新compileは明示no-default-config、MDで実依存を記録する。source/headerをprehashし実depの一致をcompile後・link後に確認する。compile済obj/compile-manifest/state/logを先に保存し、後段失敗時も削除や再compileをしない。失敗後の全recipe再実行は禁止し、保存された成功境界から個別に続行する。

成果物はunsigned SYS/INF、3obj、全実dep/全header index、tool/lib/source/argv/outputs hashとmap/log。CAT/署名/配備/押下試験は行わない。既a13 source review/ARM fixtureを再利用し、追加host fixture列やGPU buildを起動しない。候補versionを付けても入力動作成功や配備可署名済みを意味しない。

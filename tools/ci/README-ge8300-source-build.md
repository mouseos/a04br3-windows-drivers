# GE8300の単一checkout source build

既存のnative core/module/UMDフルcompile手順を再利用し、過去driver archiveの必須入力だけを除去する。
run101 source851219fc/package316/mediaには変更しない。ここで生成するものは未署名の別build成果物。

## 入力と実行

公開mouseos/a04br3-windows-driversのこのbranchをcloneし、次を実行する。

```powershell
./tools/ci/build-ge8300-from-source.ps1 -EnableCandidate
```

必要な正規toolchainはWindows x64、Python3、LLVM clang/llvm-lib/lld-link/llvm-readobj、
WDK8.1 ARM、ARM32を含むMicrosoft C++ tools（例VS2019 toolset14.29）、Windows10 SDKのUCRT。
新しいMSVCがARM32を含まない場合は`-VcToolsDirectory`で既存の対応toolsetを明示する。
ARM32のUCRTヘッダ・libが揃う既存SDKを自動選択する。`-WindowsKit10Version`で明示も可能。
SDK再構築や自動再インストールはしない。MSVC/WDK/CRTはtoolchain前提であり、このrepoへ再配布しない。
既存cache C:ディレクトリ、過去GPU SYS/DLL/lib/obj、private hive、端末固有入力を要求しない。

出力はcheckout内`build/ge8300-source`。KMD SYS、UMD DLL、限定標準client、全source/toolchain/hash manifestを残す。
署名、CAT包装、配置、reboot、実機試験はこの手順に含めない。

## source出自とDOD/legacy境界

main UMD3f8291d0を基点にKMD78ca6565のclear_engine/kmd_clearを統合し、固定317 source f77f4b07をcherry-pickする。
native fb217359の54TU/137file source依存を調べ、vendor実装を`vendor_m1_11/render_owner`へ分離した。
同dirへのinclude相対参照変更以外は元native sourceを保持し、source-mapに元commit/hash/書換えを記録する。
元vendor_m1_11のDOD/legacy用既存fileは上書きしない。共通clear_protocol/frontend_memory/native_bindingはmainを維持。
共通draw requestの形状を変えず、KMDが既存combined linkでも先に提供していたvalidatorを共有する。
新nativeが要求する親platform interface宣言だけを追加する。EDK共通差分無し。

## PCO source

`third_party/mesa-pco`はMesa0d94b3f3由来の消費source/target中立生成C・header/ARM portだけのsnapshot。
生成済みCを含むためMesonや別machineのcompile_commandsをbuild時に必要としない。
370TU（PCO/NIR/util/supportとpair）を正規ARM32 user-mode/CRT ABIで毎回compileする。
旧reference/link/build manifestやprebuilt PCO archiveは不要。Windows DDI TUへGNU private-IR bitfield ABIを適用しない。
source-planに全file hash、upstream baseline、port overlay、source生成由来を保持する。
Mesa全体の再現や一般PCO機能対応を広告しない。対応範囲は既存の限定FL10開発profileのまま。

## CI

`.github/workflows/ge8300-source-build.yml`はworkflow_dispatchのみ。
正規toolchainを準備したself-hosted Windows runner（wdk81-arm32 label）で同じsource手順を実行し、unsigned成果物を回収する。
SDK installerやsecret署名鍵、device操作、過去driver cacheをCIへ埋め込まない。
ビルド成功と実機描画成功は別。source-build-manifestのhardware_verifiedはfalseを維持する。

## 現在の検証範囲

初回full buildの5cf3898eは機能修正前のsource-build基点として成果物を保持した。
現在はユーザーの明示指示で固定317 source f77f4b07をa4b59606としてcherry-pick済み。
KMDのkmd_clear.c/kmd_objects.cは317正本と全byte一致、native fb217359/UMD3f8291d0の実装は保持する。
318のdeferred prepare実装はmain所有で、確定commitを後続cherry-pickする。現317署名package/現媒体は変更しない。
source-build生成物は未署名・未実機であり、配布・媒体化には最新機能修正との明示統合・署名・検証が必要。

現行ARM32 CRTへ名前が残らない旧stricmp呼出しは、pvr_device_info TUだけで同じ契約の_stricmpへ対応する。
生成NIRのfloat16_tローカル別名変更と合わせ、source-planにport設定を記録する。
CI artifactには全componentのbuild/compile/link manifestとPE imports/exportsを含める。

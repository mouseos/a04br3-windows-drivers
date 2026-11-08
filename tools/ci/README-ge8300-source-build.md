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

## 317統合時点の検証範囲

初回full buildの5cf3898eは機能修正前のsource-build基点として成果物を保持した。
317統合時点では、ユーザーの明示指示で固定317 source f77f4b07をa4b59606としてcherry-pickした。
その時点のKMDのkmd_clear.c/kmd_objects.cは317正本と全byte一致。native fb217359/UMD3f8291d0の実装を保持した。
318の後続統合は末尾の318 runtime統合節を参照。317検証済み出力・署名package・現媒体は変更しない。
source-build生成物は未署名・未実機であり、配布・媒体化には最新機能修正との明示統合・署名・検証が必要。

現行ARM32 CRTへ名前が残らない旧stricmp呼出しは、pvr_device_info TUだけで同じ契約の_stricmpへ対応する。
生成NIRのfloat16_tローカル別名変更と合わせ、source-planにport設定を記録する。
CI artifactには全componentのbuild/compile/link manifestとPE imports/exportsを含める。

## 318 runtime統合

固定317のfull build出力を保持した後、ユーザー指定の318 source646925e1からruntime6file
（clear_engine.c/.h、kmd_clear.c/.h、lifecycle.c、preempt.c）とhost testだけを取り込む。
cache専用build-ge8300-residency318/package helperやWinPE serviceはこの統合へ持ち込まない。
公開手順は既存fullsource wrapperを維持し、native/KMD/UMDのlayout依存はすべてsource compileする。

`-RunHostTests`を付けると同sourceの実engineをx64 MSVCでcompileしhost-only回帰を実行する。
CIでは同flagを有効化しreceipt/stdoutをartifactへ含める。端末やdriver loadは不要。
318646925e1で報告済みの「Render冒頭payload回収後のIRQ完了→同slot再予約による旧draw payload pointer上書き」は
このsource-pin統合では修正しない。host testはこの競合まで検査するものではなく、修正済み/全機能成功とは扱わない。
修正は主担当の後続commitを明示取り込みする。公開build生成物は実機候補として使用しない。

## 319 caller / 選択MDL span統合

21eec1f9のfrontend_memory.c/.hとmdl_pin.cを独立cherry-pickし、main2f29aceeのkmd_clear.c callerを統合する。
owned選択viewをnative resolve identityとして使い、独立full pinned MDLとは別物を保持する。
MdlOffsetをspan選択に一度だけ適用し、owner.bindにはoffset0を渡す。native ABI v1は保持。
source-mapにruntime/caller/spanの独立出自を記録する。native fb21/UMD3f829のsourceとDOD/legacyは保持。

公開buildは既存fullsource wrapperを使う。318/319のcache専用incremental/package helperを新たな必須入力にしない。
sidecarのreal-MSVC-SEH host modelは2span/境界/PFN/失敗/leaseをexit0で確認済み。
同modelのreceiptはプロジェクト成果物領域に保存し、publicのRunHostTestsは既存engine回帰のまま。
main319の実ARM compile/LINK/SYS-CAT署名結果を採用し、同一ARM compile試験を反復しない。
319統合commitでの新規fullsource buildはまだ実行していない。直近full build検証済み出力は317/318として保持する。
318の既報payload slot競合はこのspan/caller統合で修正したものではない。実機結果はmainのrun107以降で別判定する。

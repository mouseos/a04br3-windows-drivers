# KMDF-free native coreの実WDK ARM build

`build-ge8300-provider.ps1`はf62bc96追加のnative_owner_mdl.c/native_owner_irq.cをcompileしない。NativeBacking/NativeClearJobを1にするだけではこの2moduleの確認にならない。専用`build-ge8300-native-owner-core.ps1`を使う。これは既存2.3実証providerやINF/CAT/媒体を生成・更新しない。

## 主担当runnerでのcommand

実WDK8.1とLLVMがある42333811253のsource checkout内で、下記を実行する。sourceはhelperを含むreview済み固定commitをcheckoutし、cleanであることを必須とする。

```powershell
$fixed = '<review済みのfull source commit>'
powershell -NoProfile -ExecutionPolicy Bypass -File tools\ci\build-ge8300-native-owner-core.ps1 `
  -EnableCandidate -ExpectedSourceCommit $fixed `
  -OutputDirectory "$PWD\build\ge8300-native-owner-core-arm-r1" -LinkCheck
```

`-WdkRoot`省略時は`${env:ProgramFiles(x86)}\Windows Kits\8.1`。非標準配置なら実SDK rootを明示する。新規出力dirだけを使い、失敗後も同じdirの古いobjectsを再利用しない。全tempは出力内。副作用はsource checkout配下のCOFF/lib/linkcheck artifactとログだけ。scriptはdefault offで、未指定commitやdirty sourceを拒否する。

## 必要入力

- その固定source checkout。f62のMDL/IRQ core、554の非待機submit、既存dma_pages/windows_hwを含むこと。
- 実Windows Kits8.1のInclude/km,km/crt,sharedとLib/winv6.3/km/armのntoskrnl.lib、hal.lib、libcntpr.lib、bufferoverflowfastfailk.lib。
- PATH上のClang/llvm-lib/llvm-readobj。`-LinkCheck`にはlld-linkも必要。Clang resource-dirの実intrin.hをSDKより先に使う。Hのc1a4a81と同じobsolete __ldrexd declaration aliasを使い、偽intrinsic implementationは作らない。
- **firmware/USC assets、実機、KMDF headers/libs、署名証明書、INF、NDKは不要**。これはmodule compile/link確認であり、firmware起動やruntime試験をしない。

主なflagsはthumbv7-pc-windows-msvc、_KERNEL_MODE/_ARM_/ARM、NTDDI/WinNT0x0603、NativeClearJob=1。scalar O2/no-vectorize/no-unaligned/no-FP。NativeClearActivateやcopy-service/boot診断flagsを立てる必要はない。

## 実際にcompileする5 TU

1. dma_pages.c
2. windows_hw.c
3. vendor_m1_11/native_owner_mdl.c
4. vendor_m1_11/native_owner_irq.c
5. vendor_m1_11/windows_native_submit.c

全objectをge8300-native-owner-core.libへarchiveし、COFF machine/symbolをobjects.txtへ保存する。optional linkcheckはこれら全objectの実exportを強制して実WDK kernel libsと解決する。未解決symbol無視、偽lib、dummy DriverEntry/owner callbackは使わない。生成するowner-core-linkcheck.dllはnative/no-entryのAPI closure確認用PEであり、ロード/インストールしない。署名済SYSや完成miniportの代わりにはしない。importsはntoskrnl.exe/hal.dllだけを許可し、Win32/KMDF/user CRTへの誤依存を検出する。

build-manifest.jsonにfixed commit/clean状態、source/header/tool/WDK入力/output SHA256、compile flags/commands、linkcheck選択を保存する。回収対象はbuild.log、build-manifest.json、objects.txt、ge8300-native-owner-core.lib、各obj、およびLinkCheck時のlinkcheck-imports.txt/owner-core-linkcheck.dll。compile、API closure link、実WDDM link、実機成功を区別する。

## H13c141eとの境界

Hのnative_binding.hが宣言するGeoNativeOwnerCreate/Deleteとcontrol-v2 callback tableはまだ本coreに実装していない。helperがlib/linkcheck成功しても、Hのminiportを最終linkできたとは扱わない。shim stubで未解決を埋めない。Herschelへ渡すresource/IRQ headerはnative_owner_mdl.hとnative_owner_irq.h。modern FWIF enumとm1.11 private TUを混ぜず、DxgkだけがMOU0006 IRQを所有する構成を維持する。

残る実接続はsame-MDL native render target/prepare、real completion→cache visibility→その後のhardware IRQ、MMU/DMA drain+rundown/reset/quiesce。triangle shader/先頭PPP/PDSの未取得資源も推測で補完しない。

現時点の検証はhost OS doublesとsource review。Linux hostに実WDK8.1/PowerShell実行環境はなく、このhelperの実ARM compile/linkは主担当runnerでこれから実施する。2.4をclear反復だけのために焼かず、WDDM/triangle候補への統合後に次試験を行う方針を維持する。

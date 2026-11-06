# Wi-Fi / USB / audio 単一parent統合

Raman側の `work/rt-usb-keyboard-output` の `d9d95dc`（USB/audio統合）と
`5f81872`（USB再列挙検証）をWi-Fi側の `0b2b60c` へmergeした構成。
統合先は同一repoの `work/rt-wifi-runtime-20261107`。Ramanのworktreeは変更しない。
双方の履歴を保持し、sharedファイルの片側上書きや別parentの並行インストールを避ける。

## 必要な履歴と競合

Ramanの5f81872を基準にWi-Fiで必要なcommitは以下の順。統合branchにはすべて含まれる。

| commit | 役割 |
| --- | --- |
| 626b916 | 所有下でのcombo reset/PMU停止順序 |
| 4bfe012 | runtime校正とLinux board timing |
| 31071cb | Native PnP、国別設定、動的NVRAM、parent実lease |
| 9272c88 | country codecのbuild依存 |
| c29fcbf | PHY/MAC resetとcold-start lifecycle |
| 547eb72 | WPA2 key/port、scan probe、ARM helper依存除去 |
| 0b2b60c | 接続後の再scan管理フレーム配送 |

実際のmerge競合は `Makefile` の追加target群と
`tools/ci/build-msdc-parent-probe.ps1` のparent compile行。
Makefileは既存USB/audioとWi-Fiのtargetを両方保持。
compile行はUSB_ONLY、FIRST_PLAYBACK、DIGITAL_PLAYBACKとWIFI_RUNTIME/RTC/IRQをすべて保持し、
全parent TUの `-O2 -fno-stack-protector` を統一する。

意味上の重複ファイルは `drivers/bus/mt8167-shared/wdf_parent_probe.c`。
自動merge後、RamanのUSB所有権、既存touch/buttons、retained audioのcallbackと子PDOを維持し、
Wi-Fiの世代付きlease、RTC32K取得/解放、MSDC2子IRQを追加した。
`wifi_interface.h` はv2を維持し、実leaseの説明のみ更新。別ABIは作らない。
PMICのRTC処理は既存 `drivers/input/a04br3-buttons/pmic_read.[ch]` を利用する。

## 主担当のWDK8.1ビルド

clean checkoutのrepoルートから実行する。実機・runner起動・署名・配置はこのscriptには含めない。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ci\build-mt8167-first-playback.ps1 -IntegratedUsb -IntegratedWifi -OutputDirectory .\build\mt8167-usb-audio-wifi
```

既存first-playbackのフラグ/音声経路を継承する。IntegratedWifiだけの指定は拒否し、
USB_ONLY=0、touch D0=1、buttons D0=1、APMIXED=1に加えて、
WIFI_RUNTIME=1、RTC32K=1、WIFI_IRQ=1を明示する。WIFI_HANDOFF診断は0。
既存のoptional thermal指定はIntegratedUsbと同じく継承する。

1. parentを1回だけbuildし、`a04-msdc-parent-probe/mt8167-shared.sys` を生成。
2. 同じcheckoutのretained first-playback audioとUSB SYSをbuild。
3. `build-mt6630-runtime.ps1 -SharedParentDirectory ...` でWi-Fi SYS/probeをbuild。
   この引数ではparentを再buildしない。parentのfeature manifest、commit、SYS SHA256を照合する。
4. 出力配下のparent SYSが厳密に1個であることを確認し、全成果物hashをmanifestへ記録。
   古い別構成のparentが残っていれば、新しい出力ディレクトリでbuildする。

必要dependenciesはWDK8.1 KM/shared/UM ARM headersとndis/ntoskrnl/hal/
portcls/bufferoverflowfastfailk/kernel32/advapi32 libraries、KMDF1.13、LLVM clang/clang++/
lld-link/llvm-readobj、Windows PowerShell、git。
Wi-Fi firmwareは従来どおり固定digestのsnapshotを主担当media生成工程で用意する。
MAC/校正値はeMMC既存サービスのread-only動的読出しを維持し、利用者の抽出は不要。

## 維持するPnP構成

既存ACPI\BEN0001 / Mt8167Sharedサービスに対するparent SYSは1個だけ。
USB専用ACPIへ置換せず、既存touch、battery、buttonsのnamespace/INF/serviceを維持する。
audioはRamanのfirst-playback package、USBはmt8167-usb20、Wi-FiはMT6630実ハード名の
mt6630-runtime.infを使う。battery packageやtouch SYSをこのscriptで上書きしない。
新しい共通EDK2変更はない。

出力はparent/INF/parent-manifest、audio/、usb/、wifi/、全体manifest。
parentのfeature manifestはcompileに使用した値のみを保存する。
Wi-Fi manifestは実際に再利用したparentのpath/hashを保存する。
SSID/BSSID/鍵/PSK/個体校正のログや成果物は生成しない。

## 確認範囲とRaman/主担当への連絡

既存test-mt8167-usb-integrated、test-mt6630、test-mt6630-firmwareを一括確認した。
追加模擬test群は作らない。変更したPowerShell 4 scriptは実Parserで構文確認。
統合フラグのparentを公開WDK10 10.0.16299 / KMDF1.15参照headersでARM32 COFF compileした。
parent link対象9 COFFはARM runtime/GS helperの未解決参照なし。
既存のcompile-only codec_adc_probe.objには__rt_sdiv64参照が残るが、parent scriptのlink対象外。
これはWDK8.1/KMDF1.13最終linkの証明ではない。

Raman側へ渡す単位はこのmerge済みbranch全体。5f81872からのmergeでWi-Fi履歴を再適用せず採用できる。
直接のRaman宛てcollaboration toolは本セッションに公開されておらず、対話による採用確認は未了。
主担当へ同じcommit/手順を引き渡し、Ramanの後続変更があれば統合parentを片側版へ戻さずmergeする。

主担当は最新WDK8.1の一括link/署名と通常OS配置を行う。PnP資源、touch/battery/buttonsの継続、
audio/USB/Wi-Fiの同居、NVRAM動的読出し、実scan/WPA2接続は実機確認が残る。
実機/runnerは主担当専有、REAはEinstein専有、USBカメラによる観測は行わない。

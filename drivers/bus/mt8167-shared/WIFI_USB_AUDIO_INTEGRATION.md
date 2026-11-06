# Wi-Fi / USB / audio 単一parent統合

最終統合の所有branchは`work/rt-peripherals-integrated-20261107`。
Nashの確定`331cb2f`を基点に、audio `ae1c716`（v11・直接KS client）を
統合した。最終成果はこの基点に対する1コミットに固定する。
Nashのworktree、実機、runner、UEFI、Einstein所有REAは変更しない。
UEFI `cc7c20fa`のboot生成済みという主担当の情報を参照記録に留める。

以下の履歴表はNashがUSB/audio v10とWi-Fiを統合した時点の出自。
最終ソースではv11とKS client/decoderを保持し、親の別構成を上書きしない。

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

以下の段落はNashの331cb2f時点の確認記録。今回のaudio v11統合後のPowerShell/WDK実行を意味しない。
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


## 最終profileとpackage version

一括入口`build-mt8167-peripherals.ps1`はcleanな固定commitだけを受け付け、
内部で`build-mt8167-first-playback.ps1 -IntegratedUsb -IntegratedWifi`を
一度実行する。Wi-FiはNashの`SharedParentDirectory`契約を使い、親の
capability manifest・commit・SHA256を照合して再利用する。親SYS数は厳密に1個。
親/audio/USB/Wi-FiとKS client/WLAN probe、対応INFの全hashを最上位manifestへ残す。
失敗時は成功manifestを作らず`build-state.json`をfailedにする。古いmanifestは保存する。

同梱4 INFの固定値は`DriverVer=11/06/2026,2026.11.6.1001`。
parentの既存oem50/0.0.1.xに対してversionを明確に上げ、同rank同versionによる
旧package残留を避ける。UTC 11/06のrunnerで11/07がfuture dateになった実例に
合わせ、audioを含め11/07は同梱INFに残さない。sourceと生成先INFの値を照合し、
日付/versionを親・各child・最上位manifestへ記録する。
全INFのDDInstall/Servicesは`Install.NT`/`Install.NT.Services`装飾を維持する。
Device/DisplayNameはMediaTek MT8167/MT6392/MT6630の実ハード名で、
experimentalやa04br3を表示名に使わない。既存hardware IDとサービス名は変えない。

version更新はpackage選択の条件であり、現在常駐する旧SYSが置き換わったという
証拠ではない。署名・PnP更新・必要な再起動と、選択INF/DriverVer/SYS hashの
照合は主担当の実機工程。こちらではInf2Cat・署名・配置・実機操作を実行していない。

音声はv11の親・Audio SYS・KS clientを同じcommitから作る。親の
FIRST_PLAYBACK/VOICE_PCM/DIGITAL_PLAYBACK/MT6392_CLK_ALIAS/UNMUTEは1、
旧SHARED_VOICE_DIAGNOSTICは0。子のFIRST/DIGITAL/SILENT_DMA/PCM_PATTERN/UNMUTEは1、
AUDIO_VOICE_PCM_DIAGNOSTICは0。KS clientはRUN一度→snapshot→STOP要求後に
既存TOPRGU queueへ観測を送る。AudioSrv不要、対象ARM32 ksuser.dllが必要。
可聴成功は未確認。通常OSのGPU/DODと一回限りPnP installerは主担当が所有する。

既存touch/battery/buttons/GPUのINF・namespace・resource validatorを変更しない。
PC接続/外部VBUS/ID-deviceでは、USBのsource eligibilityが無書込みで拒否し、
親・audio/Wi-Fi leaseをpoisonしない。GPIO/clock/PMICの既存bit maskを広げない。
単一parentでの同時フラグ、audio leaseとRTC lease、USB条件拒否を既存fixtureに
まとめて確認するが、実PnP・実Wi-Fi接続・実キー入力・音声再生成功とは区別する。

最終audio v11統合後は`make test-mt8167-peripherals`が成功。4 INFの固定日付/版、NT装飾、実ハード表示名を静的確認した。ローカルにPowerShellがないため、更新後scriptの実Parser確認およびWDK8.1 link/Inf2Catは未実施で、主担当の一括build工程へ渡す。

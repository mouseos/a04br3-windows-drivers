# MT8167 keypad / MT6392 keys

CT3 boardで確認できるVolume UpとHomeを既存MT6392 Power/Volume DownのHID子へ追加する置換候補。ALS/proximityのICやI²C接続はboard sourceから確認できないため、そのドライバは追加しない。

## 一次sourceと所有権

CT3 Linux `7c4782127d4ea11965be00ce1343056be9bfb769`：

- `arch/arm64/boot/dts/mediatek/a04br3.dts` keypad: map index0=115(VolumeUp)、index1=102(Home)、debounce1024。
- `arch/arm64/boot/dts/mediatek/mt8167.dtsi`: `keypad@10002000`、長さ0x1000。
- `drivers/input/keyboard/mediatek/mt8167/hal_kpd.h/c`: 16bit MEM1+4、DEBOUNCE+0x18(14bit)、EN+0x24(0/1)、active-low decode。
- `drivers/input/keyboard/mediatek/kpd.c`: probeはclock取得を試みるが失敗でも継続。本機DTSにはkeypad clock specifierなし。
- UniXNU-a04br3 `package/A04BR3Input.cpp` のkeyThreadもdebounce1024→enable1→MEM1 readを使用。

`MT6392\Buttons` の既存exclusive resourceだけをmapする。新PDOやI²C/PMIC/TOP/INFRA/GPIO/IRQ ownerは追加しない。PMICは既存v1 `ButtonPmicRead`のみ。親source/API/資格tokenは変更しない。

D0 entryは元EN/debounceを保存し、上記二設定だけをwrite/readbackする。ENが0/1以外、debounce上位2bitが非ゼロなら書かない。設定不一致なら復元しkeypad入力を停止。D0 exitはtimerを同期停止してから元EN/debounceを復元し、全release、queue purge。ReleaseHardwareでunmapする。元EN=1の場合は元の走査状態へ戻すため、全SoC走査停止を保証する契約ではない。

clock/pinmuxの制御値は推測しない。readbackは設定一致を示すだけでscanner進行や実clockの証明ではない。継承clock/pinmuxで実入力できることは未検証。no IRQ/wake、省電力inputは未実装。

## HID契約

4つの独立した絶対値2byte report。起動時はrelease、各系統で3回の安定releaseを確認してから入力をarmする。20ms polling、3安定sample debounce。keypadとPMICの失敗は独立してrelease/latchし、その系統を次のD0まで停止する。別系統の入力は継続する。

| ID | Usage | 意味 |
| --- | --- | --- |
| 1 | Generic Desktop/System Power Down | 既存Power、OS設定による電源動作の可能性あり |
| 2 | Consumer/Volume Decrement | 既存Volume Down |
| 3 | Consumer/Volume Increment | Volume Up |
| 4 | Keyboard/Left GUI (0x07/0xe3) | 標準Windowsキー。実キー・Start画面の動作は未検証 |

保持状態だけを公開し、toggle injection、synthetic repeat、偽姿勢、ユーザー操作の生成はしない。短い押下やqueue滞留中の変化はcoalesceされうる。

## 再現ビルド

既存の回収済WDK8.1 headers/ARM libs/KMDF1.13小closureを使う。SDKのinstall、親・GPU・UEFI再buildは不要。

```
python3 tools/ci/cross-build-mt8167-keypad.py --cache /ROOT/回収cache --output "$PWD/build/keypad"
```

cacheは既存cross-build-mt6392-keys.pyと同じ `sdk-headers/wdk/Include`、`compiler-libs/wdk/Lib`構造。clang/lldのARMNTで3 TUだけcompile/linkし、全source/obj/lib/unsigned outputのSHA256とCLIをmanifestへ保存する。TMPDIRは指定output内。

Windows既存署名hostでは `tools/ci/sign-mt8167-keypad.ps1`を使う。署名済SYS/CATのsignerとCATのSYS/INF membership、ARMNTを検証する。鍵exportは不要。

## 後段採用

INF0.0.1.2は同じHWID、同じLowerFilters/service `Mt6392Keys`を維持し、ServiceBinaryだけ実ハード名`mt8167-keypad.sys`へ置換する。旧mt6392-keys.sysと同時loadしない。既存媒体への適合・service停止/driver選択はmain担当。今回のfreeze済batchを更新しない。親再build不要。現親が既存buttons子を公開しexclusive rangeを与えることが必要。

host decode/debounce試験と署名buildは実キー入力の成功証拠ではない。実機試験はmain所有の次の通常更新で行う。

## Windowsキーへの修正（0.0.1.2）

旧候補のConsumer AC Homeはブラウザ系のHomeであり、Windowsキーのusageではない。第4reportだけGeneric Desktop Keyboard collection内のKeyboard Left GUIへ変更する。report ID4、1bit状態＋7bit padding、全2byte、既存Power/Volumeの3reportとdebounce/PMIC/MMIO/停止処理は保持する。

一次資料：[Microsoft Keyboard Input Overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input)はKeyboard Left GUIをpage0x07/usage0xe3、Scan1 E05Bへ対応させる。[ACPI button device](https://learn.microsoft.com/en-us/windows-hardware/drivers/hid/acpi-button-device)もWindows/Homeに同usageを使用するが、後者はWindows10用ACPI例であり、RT8.1へACPI0011やHidinterrupt.sysを持ち込む根拠にはしない。既存mshidkmdfと標準kbdhidへ通常のkeyboard HID collectionを公開する。実機のkbdhid結合/キー入力/Startの動作確認までは未検証。

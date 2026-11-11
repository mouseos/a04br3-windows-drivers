# Buttons lifecycle checkpoint v1

実227ではImagePath/Start/LowerFilterの静的設定だけを確認した。DriverEntry、PrepareHardware、D0、HID列挙、入力配送の到達点は未確定。この変更は原因修正ではなく、次の通常更新に同梱する最小観測である。

## 保存と読取り

保存先はDriverEntryへ渡された自身のservice path配下 `Parameters\NativeCheckpoint`。想定実パスは `SYSTEM\ControlSet001\Services\Mt6392Keys\Parameters\NativeCheckpoint`。固定値名 `LoadEpochV1` と `B01`〜`B24` のREG_BINARYのみ。別service、WDF設定、受理条件、HID descriptor、失敗latch、MMIO操作回数を変更しない。

全ULONGはlittle-endian。`LoadEpochV1` は16byteで `[magic=0x31424b41, version=1, epochLow, epochHigh]`。epochはDriverEntryのKeQuerySystemTime値。各Bxxは80byte、20ULONGで次の形式。

| Index | 意味 |
| --- | --- |
| 0..2 | magic、version=1、bytes=80 |
| 3..5 | stage、kind、raw status |
| 6..11 | stage別aux6 |
| 12..13 | load epoch low/high |
| 14..15 | event FILETIME low/high |
| 16..19 | reserved=0 |

kindは0=status未適用、1=NTSTATUS、2=boolean、3=parent backendのint結果（32bitのまま保存）。成功/失敗の解釈にkindを使い、backend値をNTSTATUS扱いしない。

readerは長さ/magic/version/stageと値名/予約領域/kindを確認し、**LoadEpochV1と一致するrecordだけ**同loadとして扱う。旧epoch、欠落、不正recordは未観測。driverが一度もloadされなければ前回epoch自体も残るため、epoch一致だけで今回bootと断定せず既boot証拠と時刻を照合する。registryの永続化や書込み成功は保証せず、記録欠落を未実行と断定しない。

## stageとaux

表で省略したauxは0。

| Bxx | stage | kind | aux（順） |
| --- | --- | --- | --- |
| 01 | DriverEntry | 0 | なし |
| 02 | WdfDriverCreate結果 | 1 | なし |
| 03 | DeviceAdd開始 | 0 | なし |
| 04 | DeviceAdd結果 | 1 | なし |
| 05 | Prepare開始 | 0 | raw数、translated数 |
| 06 | raw先頭descriptor | 0 | Type、ShareDisposition、memory開始low/high、memory長、descriptor存在 |
| 07 | translated先頭descriptor | 0 | B06と同形式 |
| 08 | QueryInterface結果 | 1 | Size、Version、Context存在、Reference存在、Dereference存在、ButtonPmicRead存在 |
| 09 | Prepare結果 | 1 | なし |
| 10 | D0開始 | 0 | previous state、parent Context存在 |
| 11 | ScanStart結果 | 2 | reason、saved enable、saved debounce、scan owned |
| 12 | D0結果 | 1 | なし |
| 13 | PMIC初回domain失敗 | 3 | なし（raw statusに実backend戻り値） |
| 14 | keypad初回domain失敗 | 2 | なし（raw status=0、検証失敗） |
| 15 | HID descriptor応答 | 1 | IOCTL、outlen、inlen |
| 16 | report descriptor応答 | 1 | IOCTL、outlen、inlen |
| 17 | read要求/forward結果 | 1 | IOCTL、outlen、inlen |
| 18 | read初回配送試行 | 1 | report ID、論理button状態、成功時byte数 |
| 19 | D0Exit完了 | 1 | next state |
| 20 | ReleaseHardware完了 | 1 | なし |
| 21 | GET_INPUT_REPORT結果 | 1 | IOCTL、outlen、inlen |
| 22 | device attributes結果 | 1 | IOCTL、outlen、inlen |
| 23 | read初回Copy失敗 | 1 | report ID |
| 24 | read初回forward失敗 | 1 | なし |

B06/07は元のcount==1検証を通過した場合だけ記録。count不正時はB05/B09で判断する。Scan reasonは0=設定一致、1=未map、2=初期EN/debounce不適合、3=書込み後readback不一致。追加のMMIO readは行わない。read配送は起動時releaseを含み、実押下やOSによるキー消費の証拠ではない。B08成功もinterfaceの後段形式検証成功とは限らずB09と合わせる。

## thread/IRQLと上限

各stageはdriver load内でInterlockedCompareExchangeにより一度だけclaim。PASSIVE_LEVELかつspecial APC有効時のみ、既wait lockの外で同期保存する。条件不適合時は保存しない。保存失敗はretryせず元のDDI結果を維持する。最大24record＋epoch1件、追加worker/timer/flushなし。ポーリングは初回domain失敗だけ保存し、正常sampleごとには書き込まない。descriptor/readについても各stage初回のみ。通常の失敗latch、timer停止、queue purge、親referenceの所有権を変更しない。

## 必要TUと既recipeの再利用案（実行前）

変更TUは `wdf_keys.c` 1件。`checkpoint.h` はそのTUだけがincludeする。`button_state.c` / `button_pipeline.c` と各header、shared parentのABIは無変更。

既入口は `tools/ci/cross-build-mt8167-keypad.py`（回収WDK8.1/KMDF1.13、clang ARMNT/lld）と `tools/ci/sign-mt8167-keypad.ps1`（既BCBF署名、CAT membership/ARMNT検証）。**現在のbuild入口は3TU compile固定であり、そのまま1TU buildとは呼ばない。** 既recipeのflags/libsを再利用する増分経路では、保持2objectの実hashと元source/header/tool/include closureを確認してcopyし、wdf_keysだけcompile、3objectでlink1回、既署名入口へ渡す。SDK/親/UMD/transportの再buildは不要。

再利用する実selected0.0.1.2のsourceは `7bb166c71475be286cfa13478f0456a67210c817`。元manifestのobject SHA256は以下。

| object | SHA256 |
| --- | --- |
| button_state.obj | 009405ef821add8eae38712d686bbfa41bf5c9a26d7fa8e720ea56d825e1553b |
| button_pipeline.obj | f766eee193c6ba31d9e79b87be476ed0c78c1078bf98b9ff89c54275a416afeb |

現回収のsigned ZIP/sign-input ZIPはpackageのみで2objectを含まない。root内の該当objectも未確認。保持objectと元include closureを確保するまでは増分1TU実行を開始せず、無断で3TU再compileへ切り替えない。候補version/date/INF pinと実行時期はmain調整後に確定する。このsourceではINFを変更せず、候補のbuild/sign/実機適用は未実施。

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

## 1.3候補の正式増分入口

保持objectはgitignore対象の旧build内で発見し、上記2SHAと一致確認した。先の未確認記録は探索時点の状態として残す。`tools/ci/build-mt8167-keypad13-incremental.py --baseline RETAINED12 --cache RETAINED_WDK --output NEW_OUT` は実1.2manifest固定SHA、2object/source/header、6libを照合し1TU compile/link1回のみ行う。Windows LLVMの実versionと全再結合headerのSHAを記録し、旧Linux compiler/header closureと同一とは主張しない。Windowsのcase-insensitive pathで旧VFS overlayを置換する。既署名入口は1.2を維持して明示schemaの1.3も受理する。INFは日付11/10/2026・版0.0.1.3で識別し、名前/HWID/service/KMDF/LowerFilterは不変。

## 入力境界の有限追加観測（source候補、未build/未配備）

本追記は3414de13の実1.3を基点とする観測のみの候補。B01..24のlayout/kind/意味、LoadEpochV1、80B record、reserved4DWORD=0、first-onlyを保持し、B25..31の7eventだけを追加する。最大31record＋epoch=32値。INF/versionは変更せず、同version別SYSの配備を承認するものではない。動作修正・入力成功とは扱わない。

| Stage | kind/status | 内容 |
| --- | --- | --- |
| B25 | 0/0 | online timer内の初回sample後snapshot。timerのentryだけでなく既sampleまで進んだ事実。全系統validという意味ではない |
| B26 | 2/1 | PMIC domainのarmedが0→1となった初回sample後snapshot |
| B27 | 2/1 | keypad domainのarmedが0→1となった初回sample後snapshot |
| B28..31 | 1/Copy実NTSTATUS | 各report ID1..4の値1についてCopy成功、既WdfRequestComplete(request,s)呼出しが復帰した初回だけ |

B25..27は共通aux6である。

| aux index | 数値と適用条件 |
| --- | --- |
| 0 | このcallbackで既存ButtonPmicReadが扱ったUSHORT raw。未試行時の初期0は読取値ではない。backend失敗時のrawは有効値ではない |
| 1 | このcallbackの既MEM1値。kpvalidでなければ初期0xffffで、読取値ではない |
| 2 | flags: bit0 PMIC呼出し試行、bit1 PMIC結果valid、bit2 keypad読取前eligible（未failedかつscan_owned）、bit3 keypad設定検査通過/MEM1取得、bit4 online。上位0 |
| 3 | 既PMIC backend int返却値をULONGへ変換した32bit。bit0が0なら初期0で、backend成功ではない。NTSTATUSではない |
| 4 | sample後PMIC domain pack |
| 5 | sample後keypad domain pack |

各domain packは`known[3:0] | armed<<8 | failed<<9 | value<<16`。他bit=0。PMIC known/valueはmask9（Power0/VolumeDown3）、keypadはmask6（Home1/VolumeUp2）。knownはdebounceの既知bitであり、armedと分離。初回arm時は当該domainで全既知・release・failed0のためPMIC pack265(0x109)、keypad pack262(0x106)。rawは追加読取りせず既sampleのローカル値だけを使用する。flagsとpackはwait lock内でcopyし、Zw呼出しを含む保存はlock解放後。timer再arm条件/20ms、MMIO/PMIC操作、state/arming/failure latchは不変。

B28..31 auxは`[reportID,1,2,3,0,0]`。aux3のbit0はCopyのNT_SUCCESS、bit1はWdfRequestCompleteの呼出しが復帰した事実。WdfRequestCompleteはVOIDであり独立した完了成功statusは取得できない。statusにはCopy実NTSTATUSだけを入れる。requestへの最後のaccessは元のCompleteであり、その後はlocal id/data/sだけを読む。HIDclass受信・kbdhid・OSによるキー消費は未証明。B18の従来の初期releaseを含む最初の配送試行は別に保持する。

全新eventも既保存関数のPASSIVE_LEVEL/APC有効検査を通り、Interlockedの1回claimで最大1writeだけ。保存失敗のpoll retryやflush追加はない。B26/27はload内初回armのみであり後続D0は上書きしない。欠落/staleから未呼出し・未arm・未押下を断定しない。epoch一致は同loadだけで、今回boot保証には既boot境界との外部結合が必要。

`tools/recovery/read-keypad13-checkpoint-loaded.ps1`は既6687版をこの基点へ追加し、値名B01..31/最大32値、kind、snapshot pack、押下auxを検査する。既80B/epoch検査とV1 schemaは維持。`input_boundary_observation.current_load_stages`はschema validかつ同loadの追加stageだけで、成功stage一覧ではない。B25は無効sampleでも到達し得る。`current_boot_proven`と`OS_input_consumption_proven`は常にfalse。旧1.3データの追加値欠落は正常に読めるが新境界はunknown。このbranchはcallerを追加せず、次の正当な一括更新側の同load callerへreaderを統合する必要がある。

変更TUはwdf_keys.cだけ（checkpoint.h include）。reader/文書はhost側。新driverbuild/sign/採番/配備、追加load・押下依頼なし。次の通常更新に採用する前に固定sourceと1TU最小buildの別承認・artifact確認が必要。

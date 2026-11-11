# USB1121：現在GPUを保持する既存serviceの再有効化

基点は`4939c830`。既USB native/media入口を拡張し、旧新規service作成を既定のまま残す。
service名とU198の198はprotocol identityであり、capture番号ではない。
現在のcaptureから同一GPUのままUSB単体を導入する経路であり、GPU更新と同じ起動へ混ぜない。
既installer内の親1121への切替・停止確認・rollbackは維持する。共有parent再起動を含む実挙動は未検証。

## native rearm

`prepare-usb1121-system198.ps1 -RearmExistingService`が明示opt-in。
既serviceがない場合は拒否し、新規作成へフォールバックしない。opt-inなしでは既serviceを従来どおり拒否する。
実233の`MT8167Usb1121Install198`は既存・disabledであり、新serviceを作成したとは記録しない。

変更前に以下の6値を型・raw bytes・長さまで比較する。

| 値 | 型 | 必須値 |
| --- | --- | --- |
| Type | REG_DWORD | 16 |
| Start | REG_DWORD | 4 |
| ErrorControl | REG_DWORD | 1 |
| ImagePath | REG_SZ | `"C:\Windows\System32\MT8167Usb1121Install198.exe"` |
| ObjectName | REG_SZ | `LocalSystem` |
| DisplayName | REG_SZ | `MediaTek MT8167 USB ABI3 Installation` |

文字列の末尾UTF-16 NULも照合する。既存modeの書込みはStart DWORD=2の1値だけ。
他value・subkeyを削除/作成せず、ACL/owner変更APIも呼ばない。
snapshotは全ServicesとGraphicsDriversの子孫・owner/group/DACLを比較する。
除外は対象service rootのStartだけで、既存service subtree全体を免除しない。
同serviceの追加value・子キー内のStartも比較対象。新規作成modeだけ従来の新service subtree除外を使う。

原SYSTEM trioをloadせず、専用cloneへcopyする。cloneをload→patch→snapshot→unload→reload→
再snapshot・raw identity照合→unloadする。privilege restoreの成功後にだけresult.jsonを確定する。
失敗時はfailure/cleanup receiptを残し、成功resultを先に出さない。
既存modeのschemaは`MT8167_USB1121_NATIVE_REARM_V2`、`service_created=false`、
`existing_service`、`mode=existing-disabled-start-only`、raw_before/raw_after、除外範囲、
snapshot一致、load/reload/unload/privrestoreの結果とsource/hashを記録する。`new_service`は出さない。
新規作成modeは旧`MT8167_USB1121_NATIVE198_V1`を保持し、追加fieldで作成とraw_afterを明記する。

## 入力とsourceの結合

将来の入力生成では既generatorに`--retain-current-gpu`を指定する。
従来のfresh/native/final-policy/GPU署名manifestの8 pinは引き続き必要。
ここでpolicyとGPU manifestは**capture上の現在GPU**を表す。将来のGPU候補を指定しない。
GPUのSYS/transport/UMD/FWのhashが実capture witnessと一致しない場合は拒否する。
一次captureにUMD/FWがなければ、`--gpu-capture-manifest`と`--gpu-capture-sha256`で
同fresh SHA・同Recovery bootの実read-only/unmounted補足captureを指定する。
旧USB EXEのcaptureも必須。実233ではこのwitnessがなく、配備入力生成の根拠にならない。

profileは`standalone-retained-gpu`、`gpu_update=false`、`current_gpu_capture_verified=true`を持つ。
provenanceに原fresh manifest/native receipt/policy/必要時GPU補足manifestをbyte同一で保存し、
input closureへ結合する。capture番号の改名や、old198 receiptのラベル変更では一致しない。
component recipe/launcherもこのphaseを識別する。runtime C・署名driver/helper・公開ABIは不変。
GPU更新用の`after-final-gpu-before-final-flush`は別経路であり、rearm媒体には使用しない。

## mainが将来実行する順序

以下は入力固定とmainの合図後の手順であり、今回実行していない。
既generatorで実入力を生成し、既component recipeで同一commitからSCM1TUのみbuild/signする。
SDK・署名driver/helper・GPUを再buildしない。次に同じsourceのnative入口を使う。

```powershell
& "$src\tools\recovery\prepare-usb1121-system198.ps1" `
  -Inputs $CurrentInputs -ExpectedInputClosureSha256 $CurrentClosureSha256 `
  -RearmExistingService -OutputDirectory "$src\out\usb-current-rearm"
```

実captureの原SYSTEM trioが不変であること、cloneのresultと全出力を回収・hash照合してから、
既`build-usb1121-fresh198-media.py`を明示modeで利用する。

```sh
python3 -B "$src/tools/winpe/build-usb1121-fresh198-media.py" \
  --mode existing-service-rearm --workspace "$root" \
  --inputs "$CURRENT_INPUTS" --expected-input-closure-sha256 "$CURRENT_CLOSURE_SHA256" \
  --signed-package "$SIGNED_COMPONENT" --expected-service-manifest-sha256 "$COMPONENT_MANIFEST_SHA256" \
  --native-directory "$RECOVERED_NATIVE" --expected-native-result-sha256 "$NATIVE_RESULT_SHA256" \
  --fresh-manifest "$ACTUAL_CAPTURE_MANIFEST" \
  --base "$GUARDED_BASE" --tools "$MEDIA_TOOLS" --out "$NEW_MEDIA_OUTPUT"
```

このmodeはcomponent-manifest.jsonのsource/payload/hash、実freshとprovenance、
明示rearm result、現在GPU保持witnessを検証する。native結果のraw before/afterも独立に照合する。
既native7ec/SYSTEMやold198署名componentの改名は許可しない。
`--validate-only`は検証JSONだけを作り、WIM/ESPを生成しない。
旧CLIは`legacy-new`が既定で、旧fresh198/native7ecの厳密pinを維持する。

current媒体は旧USB EXEを含むcapture済みbaseline全ファイルを実端末と比較し、
新payloadの上書き先には実captureまたは明示absent witnessを要求する。
既transaction directoryがある場合は拒否し、過去journalをResumeしてしまう配備はしない。
既ログやtransactionを勝手に削除せず、mainが新capture前に適切に保全する。
既service directory/EXEの存在を要求し、payloadを比較してから最後にSYSTEMを更新する。
GPU/他driverをcopyせず、新しいcombined serviceも作らない。
既controllerのrun/PID結合START/result/SCM_STOP/FLUSH/TERMINAL gateを維持する。

## 今回の検証と未実施範囲

host 1バッチでPowerShell27ケース、Python媒体validator44ケースが通過。
PowerShellは製品のPatchService/Snapshot関数をASTから取り出し、製品C#のraw identity/hashを使う。
Registry API境界だけメモリキーに置換し、実233の生6値から新規/既存/Start違い/identity違い/
追加value・child保持・ACL変化・他Services/GraphicsDrivers変化を検証した。
媒体側は製品validatorを呼び、raw・schema・同fresh・source・cleanup・witness欠落・旧component拒否、
実233と同fresh補足による現在GPU379の保持を検証した。
contract fixtureはテストのメモリ内だけで扱い、実native receipt/署名manifestとして出力していない。

Linux portable PowerShell 7.4.13上のhost検証であり、Windows PowerShell/native APIの動作証明ではない。
ACL取得API、clone load/reload/unloadとprivilege restore、実payload上書き、起動・USB導入・
共有parent再起動・列挙・給電改善は未検証。actual235、現入力に結合した署名payload/native結果も未取得。
最終入力生成、native hive実行、build/sign、WIM/ESP生成、VM/REA/実機操作はすべて0。

再実行用テストは`tests/test-usb1121-current-rearm-host.ps1`と
`tests/test-usb1121-current-media-host.py`。
生ログ・実233由来raw・結果・handoffは作業rootの
`port/gsi/logs/ios-feasibility/windows-rt-usb-current-rearm/`に保存する。
